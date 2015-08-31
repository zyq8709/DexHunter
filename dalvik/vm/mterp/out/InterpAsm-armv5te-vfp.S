/*
 * This file was generated automatically by gen-mterp.py for 'armv5te-vfp'.
 *
 * --> DO NOT EDIT <--
 */

/* File: armv5te/header.S */
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
 * ARMv5 definitions and declarations.
 */

/*
ARM EABI general notes:

r0-r3 hold first 4 args to a method; they are not preserved across method calls
r4-r8 are available for general use
r9 is given special treatment in some situations, but not for us
r10 (sl) seems to be generally available
r11 (fp) is used by gcc (unless -fomit-frame-pointer is set)
r12 (ip) is scratch -- not preserved across method calls
r13 (sp) should be managed carefully in case a signal arrives
r14 (lr) must be preserved
r15 (pc) can be tinkered with directly

r0 holds returns of <= 4 bytes
r0-r1 hold returns of 8 bytes, low word in r0

Callee must save/restore r4+ (except r12) if it modifies them.  If VFP
is present, registers s16-s31 (a/k/a d8-d15, a/k/a q4-q7) must be preserved,
s0-s15 (d0-d7, q0-a3) do not need to be.

Stack is "full descending".  Only the arguments that don't fit in the first 4
registers are placed on the stack.  "sp" points at the first stacked argument
(i.e. the 5th arg).

VFP: single-precision results in s0, double-precision results in d0.

In the EABI, "sp" must be 64-bit aligned on entry to a function, and any
64-bit quantities (long long, double) must be 64-bit aligned.
*/

/*
Mterp and ARM notes:

The following registers have fixed assignments:

  reg nick      purpose
  r4  rPC       interpreted program counter, used for fetching instructions
  r5  rFP       interpreted frame pointer, used for accessing locals and args
  r6  rSELF     self (Thread) pointer
  r7  rINST     first 16-bit code unit of current instruction
  r8  rIBASE    interpreted instruction base pointer, used for computed goto

Macros are provided for common operations.  Each macro MUST emit only
one instruction to make instruction-counting easier.  They MUST NOT alter
unspecified registers or condition codes.
*/

/* single-purpose registers, given names for clarity */
#define rPC     r4
#define rFP     r5
#define rSELF   r6
#define rINST   r7
#define rIBASE  r8

/* save/restore the PC and/or FP from the thread struct */
#define LOAD_PC_FROM_SELF()     ldr     rPC, [rSELF, #offThread_pc]
#define SAVE_PC_TO_SELF()       str     rPC, [rSELF, #offThread_pc]
#define LOAD_FP_FROM_SELF()     ldr     rFP, [rSELF, #offThread_curFrame]
#define SAVE_FP_TO_SELF()       str     rFP, [rSELF, #offThread_curFrame]
#define LOAD_PC_FP_FROM_SELF()  ldmia   rSELF, {rPC, rFP}
#define SAVE_PC_FP_TO_SELF()    stmia   rSELF, {rPC, rFP}

/*
 * "export" the PC to the stack frame, f/b/o future exception objects.  Must
 * be done *before* something throws.
 *
 * In C this is "SAVEAREA_FROM_FP(fp)->xtra.currentPc = pc", i.e.
 * fp - sizeof(StackSaveArea) + offsetof(SaveArea, xtra.currentPc)
 *
 * It's okay to do this more than once.
 */
#define EXPORT_PC() \
    str     rPC, [rFP, #(-sizeofStackSaveArea + offStackSaveArea_currentPc)]

/*
 * Given a frame pointer, find the stack save area.
 *
 * In C this is "((StackSaveArea*)(_fp) -1)".
 */
#define SAVEAREA_FROM_FP(_reg, _fpreg) \
    sub     _reg, _fpreg, #sizeofStackSaveArea

/*
 * Fetch the next instruction from rPC into rINST.  Does not advance rPC.
 */
#define FETCH_INST()            ldrh    rINST, [rPC]

/*
 * Fetch the next instruction from the specified offset.  Advances rPC
 * to point to the next instruction.  "_count" is in 16-bit code units.
 *
 * Because of the limited size of immediate constants on ARM, this is only
 * suitable for small forward movements (i.e. don't try to implement "goto"
 * with this).
 *
 * This must come AFTER anything that can throw an exception, or the
 * exception catch may miss.  (This also implies that it must come after
 * EXPORT_PC().)
 */
#define FETCH_ADVANCE_INST(_count) ldrh    rINST, [rPC, #((_count)*2)]!

/*
 * The operation performed here is similar to FETCH_ADVANCE_INST, except the
 * src and dest registers are parameterized (not hard-wired to rPC and rINST).
 */
#define PREFETCH_ADVANCE_INST(_dreg, _sreg, _count) \
        ldrh    _dreg, [_sreg, #((_count)*2)]!

/*
 * Fetch the next instruction from an offset specified by _reg.  Updates
 * rPC to point to the next instruction.  "_reg" must specify the distance
 * in bytes, *not* 16-bit code units, and may be a signed value.
 *
 * We want to write "ldrh rINST, [rPC, _reg, lsl #1]!", but some of the
 * bits that hold the shift distance are used for the half/byte/sign flags.
 * In some cases we can pre-double _reg for free, so we require a byte offset
 * here.
 */
#define FETCH_ADVANCE_INST_RB(_reg) ldrh    rINST, [rPC, _reg]!

/*
 * Fetch a half-word code unit from an offset past the current PC.  The
 * "_count" value is in 16-bit code units.  Does not advance rPC.
 *
 * The "_S" variant works the same but treats the value as signed.
 */
#define FETCH(_reg, _count)     ldrh    _reg, [rPC, #((_count)*2)]
#define FETCH_S(_reg, _count)   ldrsh   _reg, [rPC, #((_count)*2)]

/*
 * Fetch one byte from an offset past the current PC.  Pass in the same
 * "_count" as you would for FETCH, and an additional 0/1 indicating which
 * byte of the halfword you want (lo/hi).
 */
#define FETCH_B(_reg, _count, _byte) ldrb     _reg, [rPC, #((_count)*2+(_byte))]

/*
 * Put the instruction's opcode field into the specified register.
 */
#define GET_INST_OPCODE(_reg)   and     _reg, rINST, #255

/*
 * Put the prefetched instruction's opcode field into the specified register.
 */
#define GET_PREFETCHED_OPCODE(_oreg, _ireg)   and     _oreg, _ireg, #255

/*
 * Begin executing the opcode in _reg.  Because this only jumps within the
 * interpreter, we don't have to worry about pre-ARMv5 THUMB interwork.
 */
#define GOTO_OPCODE(_reg)       add     pc, rIBASE, _reg, lsl #6
#define GOTO_OPCODE_BASE(_base,_reg)  add     pc, _base, _reg, lsl #6
#define GOTO_OPCODE_IFEQ(_reg)  addeq   pc, rIBASE, _reg, lsl #6
#define GOTO_OPCODE_IFNE(_reg)  addne   pc, rIBASE, _reg, lsl #6

/*
 * Get/set the 32-bit value from a Dalvik register.
 */
#define GET_VREG(_reg, _vreg)   ldr     _reg, [rFP, _vreg, lsl #2]
#define SET_VREG(_reg, _vreg)   str     _reg, [rFP, _vreg, lsl #2]

/*
 * Convert a virtual register index into an address.
 */
#define VREG_INDEX_TO_ADDR(_reg, _vreg) \
        add     _reg, rFP, _vreg, lsl #2

/*
 * This is a #include, not a %include, because we want the C pre-processor
 * to expand the macros into assembler assignment statements.
 */
#include "../common/asm-constants.h"

#if defined(WITH_JIT)
#include "../common/jit-config.h"
#endif

/* File: armv5te/platform.S */
/*
 * ===========================================================================
 *  CPU-version-specific defines
 * ===========================================================================
 */

/*
 * Macro for data memory barrier; not meaningful pre-ARMv6K.
 */
.macro  SMP_DMB
.endm

/*
 * Macro for data memory barrier; not meaningful pre-ARMv6K.
 */
.macro  SMP_DMB_ST
.endm

/* File: armv5te/entry.S */
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

/*
 * We don't have formal stack frames, so gdb scans upward in the code
 * to find the start of the function (a label with the %function type),
 * and then looks at the next few instructions to figure out what
 * got pushed onto the stack.  From this it figures out how to restore
 * the registers, including PC, for the previous stack frame.  If gdb
 * sees a non-function label, it stops scanning, so either we need to
 * have nothing but assembler-local labels between the entry point and
 * the break, or we need to fake it out.
 *
 * When this is defined, we add some stuff to make gdb less confused.
 */
#define ASSIST_DEBUGGER 1

    .text
    .align  2
    .global dvmMterpStdRun
    .type   dvmMterpStdRun, %function

/*
 * On entry:
 *  r0  Thread* self
 *
 * The return comes via a call to dvmMterpStdBail().
 */
dvmMterpStdRun:
#define MTERP_ENTRY1 \
    .save {r4-r10,fp,lr}; \
    stmfd   sp!, {r4-r10,fp,lr}         @ save 9 regs
#define MTERP_ENTRY2 \
    .pad    #4; \
    sub     sp, sp, #4                  @ align 64

    .fnstart
    MTERP_ENTRY1
    MTERP_ENTRY2

    /* save stack pointer, add magic word for debuggerd */
    str     sp, [r0, #offThread_bailPtr]  @ save SP for eventual return

    /* set up "named" registers, figure out entry point */
    mov     rSELF, r0                   @ set rSELF
    LOAD_PC_FP_FROM_SELF()              @ load rPC and rFP from "thread"
    ldr     rIBASE, [rSELF, #offThread_curHandlerTable] @ set rIBASE

#if defined(WITH_JIT)
.LentryInstr:
    /* Entry is always a possible trace start */
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    FETCH_INST()
    mov     r1, #0                      @ prepare the value for the new state
    str     r1, [rSELF, #offThread_inJitCodeCache] @ back to the interp land
    cmp     r0,#0                       @ is profiling disabled?
#if !defined(WITH_SELF_VERIFICATION)
    bne     common_updateProfile        @ profiling is enabled
#else
    ldr     r2, [rSELF, #offThread_shadowSpace] @ to find out the jit exit state
    beq     1f                          @ profiling is disabled
    ldr     r3, [r2, #offShadowSpace_jitExitState]  @ jit exit state
    cmp     r3, #kSVSTraceSelect        @ hot trace following?
    moveq   r2,#kJitTSelectRequestHot   @ ask for trace selection
    beq     common_selectTrace          @ go build the trace
    cmp     r3, #kSVSNoProfile          @ don't profile the next instruction?
    beq     1f                          @ intrepret the next instruction
    b       common_updateProfile        @ collect profiles
#endif
1:
    GET_INST_OPCODE(ip)
    GOTO_OPCODE(ip)
#else
    /* start executing the instruction at rPC */
    FETCH_INST()                        @ load rINST from rPC
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
#endif

.Lbad_arg:
    ldr     r0, strBadEntryPoint
0:  add     r0, pc
    @ r1 holds value of entryPoint
    bl      printf
    bl      dvmAbort
    .fnend
    .size   dvmMterpStdRun, .-dvmMterpStdRun

strBadEntryPoint:
    .word   PCREL_REF(.LstrBadEntryPoint,0b)

    .global dvmMterpStdBail
    .type   dvmMterpStdBail, %function

/*
 * Restore the stack pointer and PC from the save point established on entry.
 * This is essentially the same as a longjmp, but should be cheaper.  The
 * last instruction causes us to return to whoever called dvmMterpStdRun.
 *
 * We pushed some registers on the stack in dvmMterpStdRun, then saved
 * SP and LR.  Here we restore SP, restore the registers, and then restore
 * LR to PC.
 *
 * On entry:
 *  r0  Thread* self
 */
dvmMterpStdBail:
    ldr     sp, [r0, #offThread_bailPtr]    @ sp<- saved SP
    add     sp, sp, #4                      @ un-align 64
    ldmfd   sp!, {r4-r10,fp,pc}             @ restore 9 regs and return



    .global dvmAsmInstructionStart
    .type   dvmAsmInstructionStart, %function
dvmAsmInstructionStart = .L_OP_NOP
    .text

/* ------------------------------ */
    .balign 64
.L_OP_NOP: /* 0x00 */
/* File: armv5te/OP_NOP.S */
    FETCH_ADVANCE_INST(1)               @ advance to next instr, load rINST
    GET_INST_OPCODE(ip)                 @ ip<- opcode from rINST
    GOTO_OPCODE(ip)                     @ execute it

#ifdef ASSIST_DEBUGGER
    /* insert fake function header to help gdb find the stack frame */
    .type   dalvik_inst, %function
dalvik_inst:
    .fnstart
    MTERP_ENTRY1
    MTERP_ENTRY2
    .fnend
#endif

/* ------------------------------ */
    .balign 64
.L_OP_MOVE: /* 0x01 */
/* File: armv5te/OP_MOVE.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    mov     r1, rINST, lsr #12          @ r1<- B from 15:12
    mov     r0, rINST, lsr #8           @ r0<- A from 11:8
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    GET_VREG(r2, r1)                    @ r2<- fp[B]
    and     r0, r0, #15
    GET_INST_OPCODE(ip)                 @ ip<- opcode from rINST
    SET_VREG(r2, r0)                    @ fp[A]<- r2
    GOTO_OPCODE(ip)                     @ execute next instruction

/* ------------------------------ */
    .balign 64
.L_OP_MOVE_FROM16: /* 0x02 */
/* File: armv5te/OP_MOVE_FROM16.S */
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    FETCH(r1, 1)                        @ r1<- BBBB
    mov     r0, rINST, lsr #8           @ r0<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r2, r1)                    @ r2<- fp[BBBB]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r2, r0)                    @ fp[AA]<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_MOVE_16: /* 0x03 */
/* File: armv5te/OP_MOVE_16.S */
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    FETCH(r1, 2)                        @ r1<- BBBB
    FETCH(r0, 1)                        @ r0<- AAAA
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    GET_VREG(r2, r1)                    @ r2<- fp[BBBB]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r2, r0)                    @ fp[AAAA]<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_MOVE_WIDE: /* 0x04 */
/* File: armv5te/OP_MOVE_WIDE.S */
    /* move-wide vA, vB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    mov     r2, rINST, lsr #8           @ r2<- A(+)
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r2, r2, #15
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[B]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[A]
    ldmia   r3, {r0-r1}                 @ r0/r1<- fp[B]
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    stmia   r2, {r0-r1}                 @ fp[A]<- r0/r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_MOVE_WIDE_FROM16: /* 0x05 */
/* File: armv5te/OP_MOVE_WIDE_FROM16.S */
    /* move-wide/from16 vAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    FETCH(r3, 1)                        @ r3<- BBBB
    mov     r2, rINST, lsr #8           @ r2<- AA
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[BBBB]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[AA]
    ldmia   r3, {r0-r1}                 @ r0/r1<- fp[BBBB]
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r2, {r0-r1}                 @ fp[AA]<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_MOVE_WIDE_16: /* 0x06 */
/* File: armv5te/OP_MOVE_WIDE_16.S */
    /* move-wide/16 vAAAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    FETCH(r3, 2)                        @ r3<- BBBB
    FETCH(r2, 1)                        @ r2<- AAAA
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[BBBB]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[AAAA]
    ldmia   r3, {r0-r1}                 @ r0/r1<- fp[BBBB]
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    stmia   r2, {r0-r1}                 @ fp[AAAA]<- r0/r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_MOVE_OBJECT: /* 0x07 */
/* File: armv5te/OP_MOVE_OBJECT.S */
/* File: armv5te/OP_MOVE.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    mov     r1, rINST, lsr #12          @ r1<- B from 15:12
    mov     r0, rINST, lsr #8           @ r0<- A from 11:8
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    GET_VREG(r2, r1)                    @ r2<- fp[B]
    and     r0, r0, #15
    GET_INST_OPCODE(ip)                 @ ip<- opcode from rINST
    SET_VREG(r2, r0)                    @ fp[A]<- r2
    GOTO_OPCODE(ip)                     @ execute next instruction


/* ------------------------------ */
    .balign 64
.L_OP_MOVE_OBJECT_FROM16: /* 0x08 */
/* File: armv5te/OP_MOVE_OBJECT_FROM16.S */
/* File: armv5te/OP_MOVE_FROM16.S */
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    FETCH(r1, 1)                        @ r1<- BBBB
    mov     r0, rINST, lsr #8           @ r0<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r2, r1)                    @ r2<- fp[BBBB]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r2, r0)                    @ fp[AA]<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_MOVE_OBJECT_16: /* 0x09 */
/* File: armv5te/OP_MOVE_OBJECT_16.S */
/* File: armv5te/OP_MOVE_16.S */
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    FETCH(r1, 2)                        @ r1<- BBBB
    FETCH(r0, 1)                        @ r0<- AAAA
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    GET_VREG(r2, r1)                    @ r2<- fp[BBBB]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r2, r0)                    @ fp[AAAA]<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_MOVE_RESULT: /* 0x0a */
/* File: armv5te/OP_MOVE_RESULT.S */
    /* for: move-result, move-result-object */
    /* op vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    ldr     r0, [rSELF, #offThread_retval]    @ r0<- self->retval.i
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[AA]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_MOVE_RESULT_WIDE: /* 0x0b */
/* File: armv5te/OP_MOVE_RESULT_WIDE.S */
    /* move-result-wide vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    add     r3, rSELF, #offThread_retval  @ r3<- &self->retval
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[AA]
    ldmia   r3, {r0-r1}                 @ r0/r1<- retval.j
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    stmia   r2, {r0-r1}                 @ fp[AA]<- r0/r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_MOVE_RESULT_OBJECT: /* 0x0c */
/* File: armv5te/OP_MOVE_RESULT_OBJECT.S */
/* File: armv5te/OP_MOVE_RESULT.S */
    /* for: move-result, move-result-object */
    /* op vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    ldr     r0, [rSELF, #offThread_retval]    @ r0<- self->retval.i
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[AA]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_MOVE_EXCEPTION: /* 0x0d */
/* File: armv5te/OP_MOVE_EXCEPTION.S */
    /* move-exception vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    ldr     r3, [rSELF, #offThread_exception]  @ r3<- dvmGetException bypass
    mov     r1, #0                      @ r1<- 0
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    SET_VREG(r3, r2)                    @ fp[AA]<- exception obj
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    str     r1, [rSELF, #offThread_exception]  @ dvmClearException bypass
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_RETURN_VOID: /* 0x0e */
/* File: armv5te/OP_RETURN_VOID.S */
    b       common_returnFromMethod

/* ------------------------------ */
    .balign 64
.L_OP_RETURN: /* 0x0f */
/* File: armv5te/OP_RETURN.S */
    /*
     * Return a 32-bit value.  Copies the return value into the "thread"
     * structure, then jumps to the return handler.
     *
     * for: return, return-object
     */
    /* op vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    GET_VREG(r0, r2)                    @ r0<- vAA
    str     r0, [rSELF, #offThread_retval] @ retval.i <- vAA
    b       common_returnFromMethod

/* ------------------------------ */
    .balign 64
.L_OP_RETURN_WIDE: /* 0x10 */
/* File: armv5te/OP_RETURN_WIDE.S */
    /*
     * Return a 64-bit value.  Copies the return value into the "thread"
     * structure, then jumps to the return handler.
     */
    /* return-wide vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[AA]
    add     r3, rSELF, #offThread_retval  @ r3<- &self->retval
    ldmia   r2, {r0-r1}                 @ r0/r1 <- vAA/vAA+1
    stmia   r3, {r0-r1}                 @ retval<- r0/r1
    b       common_returnFromMethod

/* ------------------------------ */
    .balign 64
.L_OP_RETURN_OBJECT: /* 0x11 */
/* File: armv5te/OP_RETURN_OBJECT.S */
/* File: armv5te/OP_RETURN.S */
    /*
     * Return a 32-bit value.  Copies the return value into the "thread"
     * structure, then jumps to the return handler.
     *
     * for: return, return-object
     */
    /* op vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    GET_VREG(r0, r2)                    @ r0<- vAA
    str     r0, [rSELF, #offThread_retval] @ retval.i <- vAA
    b       common_returnFromMethod


/* ------------------------------ */
    .balign 64
.L_OP_CONST_4: /* 0x12 */
/* File: armv5te/OP_CONST_4.S */
    /* const/4 vA, #+B */
    mov     r1, rINST, lsl #16          @ r1<- Bxxx0000
    mov     r0, rINST, lsr #8           @ r0<- A+
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    mov     r1, r1, asr #28             @ r1<- sssssssB (sign-extended)
    and     r0, r0, #15
    GET_INST_OPCODE(ip)                 @ ip<- opcode from rINST
    SET_VREG(r1, r0)                    @ fp[A]<- r1
    GOTO_OPCODE(ip)                     @ execute next instruction

/* ------------------------------ */
    .balign 64
.L_OP_CONST_16: /* 0x13 */
/* File: armv5te/OP_CONST_16.S */
    /* const/16 vAA, #+BBBB */
    FETCH_S(r0, 1)                      @ r0<- ssssBBBB (sign-extended)
    mov     r3, rINST, lsr #8           @ r3<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r3)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_CONST: /* 0x14 */
/* File: armv5te/OP_CONST.S */
    /* const vAA, #+BBBBbbbb */
    mov     r3, rINST, lsr #8           @ r3<- AA
    FETCH(r0, 1)                        @ r0<- bbbb (low)
    FETCH(r1, 2)                        @ r1<- BBBB (high)
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    orr     r0, r0, r1, lsl #16         @ r0<- BBBBbbbb
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r3)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_CONST_HIGH16: /* 0x15 */
/* File: armv5te/OP_CONST_HIGH16.S */
    /* const/high16 vAA, #+BBBB0000 */
    FETCH(r0, 1)                        @ r0<- 0000BBBB (zero-extended)
    mov     r3, rINST, lsr #8           @ r3<- AA
    mov     r0, r0, lsl #16             @ r0<- BBBB0000
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r3)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_CONST_WIDE_16: /* 0x16 */
/* File: armv5te/OP_CONST_WIDE_16.S */
    /* const-wide/16 vAA, #+BBBB */
    FETCH_S(r0, 1)                      @ r0<- ssssBBBB (sign-extended)
    mov     r3, rINST, lsr #8           @ r3<- AA
    mov     r1, r0, asr #31             @ r1<- ssssssss
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[AA]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r3, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_CONST_WIDE_32: /* 0x17 */
/* File: armv5te/OP_CONST_WIDE_32.S */
    /* const-wide/32 vAA, #+BBBBbbbb */
    FETCH(r0, 1)                        @ r0<- 0000bbbb (low)
    mov     r3, rINST, lsr #8           @ r3<- AA
    FETCH_S(r2, 2)                      @ r2<- ssssBBBB (high)
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    orr     r0, r0, r2, lsl #16         @ r0<- BBBBbbbb
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[AA]
    mov     r1, r0, asr #31             @ r1<- ssssssss
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r3, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_CONST_WIDE: /* 0x18 */
/* File: armv5te/OP_CONST_WIDE.S */
    /* const-wide vAA, #+HHHHhhhhBBBBbbbb */
    FETCH(r0, 1)                        @ r0<- bbbb (low)
    FETCH(r1, 2)                        @ r1<- BBBB (low middle)
    FETCH(r2, 3)                        @ r2<- hhhh (high middle)
    orr     r0, r0, r1, lsl #16         @ r0<- BBBBbbbb (low word)
    FETCH(r3, 4)                        @ r3<- HHHH (high)
    mov     r9, rINST, lsr #8           @ r9<- AA
    orr     r1, r2, r3, lsl #16         @ r1<- HHHHhhhh (high word)
    FETCH_ADVANCE_INST(5)               @ advance rPC, load rINST
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_CONST_WIDE_HIGH16: /* 0x19 */
/* File: armv5te/OP_CONST_WIDE_HIGH16.S */
    /* const-wide/high16 vAA, #+BBBB000000000000 */
    FETCH(r1, 1)                        @ r1<- 0000BBBB (zero-extended)
    mov     r3, rINST, lsr #8           @ r3<- AA
    mov     r0, #0                      @ r0<- 00000000
    mov     r1, r1, lsl #16             @ r1<- BBBB0000
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[AA]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r3, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_CONST_STRING: /* 0x1a */
/* File: armv5te/OP_CONST_STRING.S */
    /* const/string vAA, String@BBBB */
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r2, [rSELF, #offThread_methodClassDex]  @ r2<- self->methodClassDex
    mov     r9, rINST, lsr #8           @ r9<- AA
    ldr     r2, [r2, #offDvmDex_pResStrings]   @ r2<- dvmDex->pResStrings
    ldr     r0, [r2, r1, lsl #2]        @ r0<- pResStrings[BBBB]
    cmp     r0, #0                      @ not yet resolved?
    beq     .LOP_CONST_STRING_resolve
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_CONST_STRING_JUMBO: /* 0x1b */
/* File: armv5te/OP_CONST_STRING_JUMBO.S */
    /* const/string vAA, String@BBBBBBBB */
    FETCH(r0, 1)                        @ r0<- bbbb (low)
    FETCH(r1, 2)                        @ r1<- BBBB (high)
    ldr     r2, [rSELF, #offThread_methodClassDex]  @ r2<- self->methodClassDex
    mov     r9, rINST, lsr #8           @ r9<- AA
    ldr     r2, [r2, #offDvmDex_pResStrings]   @ r2<- dvmDex->pResStrings
    orr     r1, r0, r1, lsl #16         @ r1<- BBBBbbbb
    ldr     r0, [r2, r1, lsl #2]        @ r0<- pResStrings[BBBB]
    cmp     r0, #0
    beq     .LOP_CONST_STRING_JUMBO_resolve
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_CONST_CLASS: /* 0x1c */
/* File: armv5te/OP_CONST_CLASS.S */
    /* const/class vAA, Class@BBBB */
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r2, [rSELF, #offThread_methodClassDex]  @ r2<- self->methodClassDex
    mov     r9, rINST, lsr #8           @ r9<- AA
    ldr     r2, [r2, #offDvmDex_pResClasses]   @ r2<- dvmDex->pResClasses
    ldr     r0, [r2, r1, lsl #2]        @ r0<- pResClasses[BBBB]
    cmp     r0, #0                      @ not yet resolved?
    beq     .LOP_CONST_CLASS_resolve
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_MONITOR_ENTER: /* 0x1d */
/* File: armv5te/OP_MONITOR_ENTER.S */
    /*
     * Synchronize on an object.
     */
    /* monitor-enter vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    GET_VREG(r1, r2)                    @ r1<- vAA (object)
    mov     r0, rSELF                   @ r0<- self
    cmp     r1, #0                      @ null object?
    EXPORT_PC()                         @ need for precise GC
    beq     common_errNullObject        @ null object, throw an exception
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    bl      dvmLockObject               @ call(self, obj)
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_MONITOR_EXIT: /* 0x1e */
/* File: armv5te/OP_MONITOR_EXIT.S */
    /*
     * Unlock an object.
     *
     * Exceptions that occur when unlocking a monitor need to appear as
     * if they happened at the following instruction.  See the Dalvik
     * instruction spec.
     */
    /* monitor-exit vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    EXPORT_PC()                         @ before fetch: export the PC
    GET_VREG(r1, r2)                    @ r1<- vAA (object)
    cmp     r1, #0                      @ null object?
    beq     1f                          @ yes
    mov     r0, rSELF                   @ r0<- self
    bl      dvmUnlockObject             @ r0<- success for unlock(self, obj)
    cmp     r0, #0                      @ failed?
    FETCH_ADVANCE_INST(1)               @ before throw: advance rPC, load rINST
    beq     common_exceptionThrown      @ yes, exception is pending
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
1:
    FETCH_ADVANCE_INST(1)               @ advance before throw
    b      common_errNullObject

/* ------------------------------ */
    .balign 64
.L_OP_CHECK_CAST: /* 0x1f */
/* File: armv5te/OP_CHECK_CAST.S */
    /*
     * Check to see if a cast from one class to another is allowed.
     */
    /* check-cast vAA, class@BBBB */
    mov     r3, rINST, lsr #8           @ r3<- AA
    FETCH(r2, 1)                        @ r2<- BBBB
    GET_VREG(r9, r3)                    @ r9<- object
    ldr     r0, [rSELF, #offThread_methodClassDex]    @ r0<- pDvmDex
    cmp     r9, #0                      @ is object null?
    ldr     r0, [r0, #offDvmDex_pResClasses]    @ r0<- pDvmDex->pResClasses
    beq     .LOP_CHECK_CAST_okay            @ null obj, cast always succeeds
    ldr     r1, [r0, r2, lsl #2]        @ r1<- resolved class
    ldr     r0, [r9, #offObject_clazz]  @ r0<- obj->clazz
    cmp     r1, #0                      @ have we resolved this before?
    beq     .LOP_CHECK_CAST_resolve         @ not resolved, do it now
.LOP_CHECK_CAST_resolved:
    cmp     r0, r1                      @ same class (trivial success)?
    bne     .LOP_CHECK_CAST_fullcheck       @ no, do full check
.LOP_CHECK_CAST_okay:
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_INSTANCE_OF: /* 0x20 */
/* File: armv5te/OP_INSTANCE_OF.S */
    /*
     * Check to see if an object reference is an instance of a class.
     *
     * Most common situation is a non-null object, being compared against
     * an already-resolved class.
     */
    /* instance-of vA, vB, class@CCCC */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r3)                    @ r0<- vB (object)
    and     r9, r9, #15                 @ r9<- A
    cmp     r0, #0                      @ is object null?
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- pDvmDex
    beq     .LOP_INSTANCE_OF_store           @ null obj, not an instance, store r0
    FETCH(r3, 1)                        @ r3<- CCCC
    ldr     r2, [r2, #offDvmDex_pResClasses]    @ r2<- pDvmDex->pResClasses
    ldr     r1, [r2, r3, lsl #2]        @ r1<- resolved class
    ldr     r0, [r0, #offObject_clazz]  @ r0<- obj->clazz
    cmp     r1, #0                      @ have we resolved this before?
    beq     .LOP_INSTANCE_OF_resolve         @ not resolved, do it now
.LOP_INSTANCE_OF_resolved: @ r0=obj->clazz, r1=resolved class
    cmp     r0, r1                      @ same class (trivial success)?
    beq     .LOP_INSTANCE_OF_trivial         @ yes, trivial finish
    b       .LOP_INSTANCE_OF_fullcheck       @ no, do full check

/* ------------------------------ */
    .balign 64
.L_OP_ARRAY_LENGTH: /* 0x21 */
/* File: armv5te/OP_ARRAY_LENGTH.S */
    /*
     * Return the length of an array.
     */
    mov     r1, rINST, lsr #12          @ r1<- B
    mov     r2, rINST, lsr #8           @ r2<- A+
    GET_VREG(r0, r1)                    @ r0<- vB (object ref)
    and     r2, r2, #15                 @ r2<- A
    cmp     r0, #0                      @ is object null?
    beq     common_errNullObject        @ yup, fail
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- array length
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r3, r2)                    @ vB<- length
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_NEW_INSTANCE: /* 0x22 */
/* File: armv5te/OP_NEW_INSTANCE.S */
    /*
     * Create a new instance of a class.
     */
    /* new-instance vAA, class@BBBB */
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResClasses]    @ r3<- pDvmDex->pResClasses
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved class
#if defined(WITH_JIT)
    add     r10, r3, r1, lsl #2         @ r10<- &resolved_class
#endif
    EXPORT_PC()                         @ req'd for init, resolve, alloc
    cmp     r0, #0                      @ already resolved?
    beq     .LOP_NEW_INSTANCE_resolve         @ no, resolve it now
.LOP_NEW_INSTANCE_resolved:   @ r0=class
    ldrb    r1, [r0, #offClassObject_status]    @ r1<- ClassStatus enum
    cmp     r1, #CLASS_INITIALIZED      @ has class been initialized?
    bne     .LOP_NEW_INSTANCE_needinit        @ no, init class now
.LOP_NEW_INSTANCE_initialized: @ r0=class
    mov     r1, #ALLOC_DONT_TRACK       @ flags for alloc call
    bl      dvmAllocObject              @ r0<- new object
    b       .LOP_NEW_INSTANCE_finish          @ continue

/* ------------------------------ */
    .balign 64
.L_OP_NEW_ARRAY: /* 0x23 */
/* File: armv5te/OP_NEW_ARRAY.S */
    /*
     * Allocate an array of objects, specified with the array class
     * and a count.
     *
     * The verifier guarantees that this is an array class, so we don't
     * check for it here.
     */
    /* new-array vA, vB, class@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    FETCH(r2, 1)                        @ r2<- CCCC
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    GET_VREG(r1, r0)                    @ r1<- vB (array length)
    ldr     r3, [r3, #offDvmDex_pResClasses]    @ r3<- pDvmDex->pResClasses
    cmp     r1, #0                      @ check length
    ldr     r0, [r3, r2, lsl #2]        @ r0<- resolved class
    bmi     common_errNegativeArraySize @ negative length, bail - len in r1
    cmp     r0, #0                      @ already resolved?
    EXPORT_PC()                         @ req'd for resolve, alloc
    bne     .LOP_NEW_ARRAY_finish          @ resolved, continue
    b       .LOP_NEW_ARRAY_resolve         @ do resolve now

/* ------------------------------ */
    .balign 64
.L_OP_FILLED_NEW_ARRAY: /* 0x24 */
/* File: armv5te/OP_FILLED_NEW_ARRAY.S */
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type@BBBB */
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResClasses]    @ r3<- pDvmDex->pResClasses
    EXPORT_PC()                         @ need for resolve and alloc
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved class
    mov     r10, rINST, lsr #8          @ r10<- AA or BA
    cmp     r0, #0                      @ already resolved?
    bne     .LOP_FILLED_NEW_ARRAY_continue        @ yes, continue on
8:  ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    mov     r2, #0                      @ r2<- false
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveClass             @ r0<- call(clazz, ref)
    cmp     r0, #0                      @ got null?
    beq     common_exceptionThrown      @ yes, handle exception
    b       .LOP_FILLED_NEW_ARRAY_continue

/* ------------------------------ */
    .balign 64
.L_OP_FILLED_NEW_ARRAY_RANGE: /* 0x25 */
/* File: armv5te/OP_FILLED_NEW_ARRAY_RANGE.S */
/* File: armv5te/OP_FILLED_NEW_ARRAY.S */
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type@BBBB */
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResClasses]    @ r3<- pDvmDex->pResClasses
    EXPORT_PC()                         @ need for resolve and alloc
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved class
    mov     r10, rINST, lsr #8          @ r10<- AA or BA
    cmp     r0, #0                      @ already resolved?
    bne     .LOP_FILLED_NEW_ARRAY_RANGE_continue        @ yes, continue on
8:  ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    mov     r2, #0                      @ r2<- false
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveClass             @ r0<- call(clazz, ref)
    cmp     r0, #0                      @ got null?
    beq     common_exceptionThrown      @ yes, handle exception
    b       .LOP_FILLED_NEW_ARRAY_RANGE_continue


/* ------------------------------ */
    .balign 64
.L_OP_FILL_ARRAY_DATA: /* 0x26 */
/* File: armv5te/OP_FILL_ARRAY_DATA.S */
    /* fill-array-data vAA, +BBBBBBBB */
    FETCH(r0, 1)                        @ r0<- bbbb (lo)
    FETCH(r1, 2)                        @ r1<- BBBB (hi)
    mov     r3, rINST, lsr #8           @ r3<- AA
    orr     r1, r0, r1, lsl #16         @ r1<- BBBBbbbb
    GET_VREG(r0, r3)                    @ r0<- vAA (array object)
    add     r1, rPC, r1, lsl #1         @ r1<- PC + BBBBbbbb*2 (array data off.)
    EXPORT_PC();
    bl      dvmInterpHandleFillArrayData@ fill the array with predefined data
    cmp     r0, #0                      @ 0 means an exception is thrown
    beq     common_exceptionThrown      @ has exception
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_THROW: /* 0x27 */
/* File: armv5te/OP_THROW.S */
    /*
     * Throw an exception object in the current thread.
     */
    /* throw vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    GET_VREG(r1, r2)                    @ r1<- vAA (exception object)
    EXPORT_PC()                         @ exception handler can throw
    cmp     r1, #0                      @ null object?
    beq     common_errNullObject        @ yes, throw an NPE instead
    @ bypass dvmSetException, just store it
    str     r1, [rSELF, #offThread_exception]  @ thread->exception<- obj
    b       common_exceptionThrown

/* ------------------------------ */
    .balign 64
.L_OP_GOTO: /* 0x28 */
/* File: armv5te/OP_GOTO.S */
    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    /* tuning: use sbfx for 6t2+ targets */
    mov     r0, rINST, lsl #16          @ r0<- AAxx0000
    movs    r1, r0, asr #24             @ r1<- ssssssAA (sign-extended)
    add     r2, r1, r1                  @ r2<- byte offset, set flags
       @ If backwards branch refresh rIBASE
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
    FETCH_ADVANCE_INST_RB(r2)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    bmi     common_testUpdateProfile    @ (r0) check for trace hotness
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_GOTO_16: /* 0x29 */
/* File: armv5te/OP_GOTO_16.S */
    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto/16 +AAAA */
    FETCH_S(r0, 1)                      @ r0<- ssssAAAA (sign-extended)
    adds    r1, r0, r0                  @ r1<- byte offset, flags set
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    bmi     common_testUpdateProfile    @ (r0) hot trace head?
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_GOTO_32: /* 0x2a */
/* File: armv5te/OP_GOTO_32.S */
    /*
     * Unconditional branch, 32-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     *
     * Unlike most opcodes, this one is allowed to branch to itself, so
     * our "backward branch" test must be "<=0" instead of "<0".  Because
     * we need the V bit set, we'll use an adds to convert from Dalvik
     * offset to byte offset.
     */
    /* goto/32 +AAAAAAAA */
    FETCH(r0, 1)                        @ r0<- aaaa (lo)
    FETCH(r1, 2)                        @ r1<- AAAA (hi)
    orr     r0, r0, r1, lsl #16         @ r0<- AAAAaaaa
    adds    r1, r0, r0                  @ r1<- byte offset
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrle   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
    ble     common_testUpdateProfile    @ (r0) hot trace head?
#else
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
    ldrle   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_PACKED_SWITCH: /* 0x2b */
/* File: armv5te/OP_PACKED_SWITCH.S */
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
    FETCH(r0, 1)                        @ r0<- bbbb (lo)
    FETCH(r1, 2)                        @ r1<- BBBB (hi)
    mov     r3, rINST, lsr #8           @ r3<- AA
    orr     r0, r0, r1, lsl #16         @ r0<- BBBBbbbb
    GET_VREG(r1, r3)                    @ r1<- vAA
    add     r0, rPC, r0, lsl #1         @ r0<- PC + BBBBbbbb*2
    bl      dvmInterpHandlePackedSwitch                       @ r0<- code-unit branch offset
    adds    r1, r0, r0                  @ r1<- byte offset; clear V
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrle   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
    cmp     r0, #0
    bne     common_updateProfile
#else
    ldrle   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_SPARSE_SWITCH: /* 0x2c */
/* File: armv5te/OP_SPARSE_SWITCH.S */
/* File: armv5te/OP_PACKED_SWITCH.S */
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
    FETCH(r0, 1)                        @ r0<- bbbb (lo)
    FETCH(r1, 2)                        @ r1<- BBBB (hi)
    mov     r3, rINST, lsr #8           @ r3<- AA
    orr     r0, r0, r1, lsl #16         @ r0<- BBBBbbbb
    GET_VREG(r1, r3)                    @ r1<- vAA
    add     r0, rPC, r0, lsl #1         @ r0<- PC + BBBBbbbb*2
    bl      dvmInterpHandleSparseSwitch                       @ r0<- code-unit branch offset
    adds    r1, r0, r0                  @ r1<- byte offset; clear V
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrle   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
    cmp     r0, #0
    bne     common_updateProfile
#else
    ldrle   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_CMPL_FLOAT: /* 0x2d */
/* File: arm-vfp/OP_CMPL_FLOAT.S */
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * int compare(x, y) {
     *     if (x == y) {
     *         return 0;
     *     } else if (x > y) {
     *         return 1;
     *     } else if (x < y) {
     *         return -1;
     *     } else {
     *         return -1;
     *     }
     * }
     */
    /* op vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    flds    s0, [r2]                    @ s0<- vBB
    flds    s1, [r3]                    @ s1<- vCC
    fcmpes  s0, s1                      @ compare (vBB, vCC)
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    mvn     r0, #0                      @ r0<- -1 (default)
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    fmstat                              @ export status flags
    movgt   r0, #1                      @ (greater than) r1<- 1
    moveq   r0, #0                      @ (equal) r1<- 0
    b       .LOP_CMPL_FLOAT_finish          @ argh


/* ------------------------------ */
    .balign 64
.L_OP_CMPG_FLOAT: /* 0x2e */
/* File: arm-vfp/OP_CMPG_FLOAT.S */
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * int compare(x, y) {
     *     if (x == y) {
     *         return 0;
     *     } else if (x < y) {
     *         return -1;
     *     } else if (x > y) {
     *         return 1;
     *     } else {
     *         return 1;
     *     }
     * }
     */
    /* op vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    flds    s0, [r2]                    @ s0<- vBB
    flds    s1, [r3]                    @ s1<- vCC
    fcmpes  s0, s1                      @ compare (vBB, vCC)
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    mov     r0, #1                      @ r0<- 1 (default)
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    fmstat                              @ export status flags
    mvnmi   r0, #0                      @ (less than) r1<- -1
    moveq   r0, #0                      @ (equal) r1<- 0
    b       .LOP_CMPG_FLOAT_finish          @ argh


/* ------------------------------ */
    .balign 64
.L_OP_CMPL_DOUBLE: /* 0x2f */
/* File: arm-vfp/OP_CMPL_DOUBLE.S */
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * int compare(x, y) {
     *     if (x == y) {
     *         return 0;
     *     } else if (x > y) {
     *         return 1;
     *     } else if (x < y) {
     *         return -1;
     *     } else {
     *         return -1;
     *     }
     * }
     */
    /* op vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    fldd    d0, [r2]                    @ d0<- vBB
    fldd    d1, [r3]                    @ d1<- vCC
    fcmped  d0, d1                      @ compare (vBB, vCC)
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    mvn     r0, #0                      @ r0<- -1 (default)
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    fmstat                              @ export status flags
    movgt   r0, #1                      @ (greater than) r1<- 1
    moveq   r0, #0                      @ (equal) r1<- 0
    b       .LOP_CMPL_DOUBLE_finish          @ argh


/* ------------------------------ */
    .balign 64
.L_OP_CMPG_DOUBLE: /* 0x30 */
/* File: arm-vfp/OP_CMPG_DOUBLE.S */
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * int compare(x, y) {
     *     if (x == y) {
     *         return 0;
     *     } else if (x < y) {
     *         return -1;
     *     } else if (x > y) {
     *         return 1;
     *     } else {
     *         return 1;
     *     }
     * }
     */
    /* op vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    fldd    d0, [r2]                    @ d0<- vBB
    fldd    d1, [r3]                    @ d1<- vCC
    fcmped  d0, d1                      @ compare (vBB, vCC)
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    mov     r0, #1                      @ r0<- 1 (default)
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    fmstat                              @ export status flags
    mvnmi   r0, #0                      @ (less than) r1<- -1
    moveq   r0, #0                      @ (equal) r1<- 0
    b       .LOP_CMPG_DOUBLE_finish          @ argh


/* ------------------------------ */
    .balign 64
.L_OP_CMP_LONG: /* 0x31 */
/* File: armv5te/OP_CMP_LONG.S */
    /*
     * Compare two 64-bit values.  Puts 0, 1, or -1 into the destination
     * register based on the results of the comparison.
     *
     * We load the full values with LDM, but in practice many values could
     * be resolved by only looking at the high word.  This could be made
     * faster or slower by splitting the LDM into a pair of LDRs.
     *
     * If we just wanted to set condition flags, we could do this:
     *  subs    ip, r0, r2
     *  sbcs    ip, r1, r3
     *  subeqs  ip, r0, r2
     * Leaving { <0, 0, >0 } in ip.  However, we have to set it to a specific
     * integer value, which we can do with 2 conditional mov/mvn instructions
     * (set 1, set -1; if they're equal we already have 0 in ip), giving
     * us a constant 5-cycle path plus a branch at the end to the
     * instruction epilogue code.  The multi-compare approach below needs
     * 2 or 3 cycles + branch if the high word doesn't match, 6 + branch
     * in the worst case (the 64-bit values are equal).
     */
    /* cmp-long vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[BB]
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[CC]
    ldmia   r2, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r3, {r2-r3}                 @ r2/r3<- vCC/vCC+1
    cmp     r1, r3                      @ compare (vBB+1, vCC+1)
    blt     .LOP_CMP_LONG_less            @ signed compare on high part
    bgt     .LOP_CMP_LONG_greater
    subs    r1, r0, r2                  @ r1<- r0 - r2
    bhi     .LOP_CMP_LONG_greater         @ unsigned compare on low part
    bne     .LOP_CMP_LONG_less
    b       .LOP_CMP_LONG_finish          @ equal; r1 already holds 0

/* ------------------------------ */
    .balign 64
.L_OP_IF_EQ: /* 0x32 */
/* File: armv5te/OP_IF_EQ.S */
/* File: armv5te/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    mov     r0, rINST, lsr #8           @ r0<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r0, r0, #15
    GET_VREG(r3, r1)                    @ r3<- vB
    GET_VREG(r2, r0)                    @ r2<- vA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, r3                      @ compare (vA, vB)
    movne r1, #2                 @ r1<- BYTE branch dist for not-taken
    adds    r2, r1, r1                  @ convert to bytes, check sign
    FETCH_ADVANCE_INST_RB(r2)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
    cmp     r0,#0
    bne     common_updateProfile
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IF_NE: /* 0x33 */
/* File: armv5te/OP_IF_NE.S */
/* File: armv5te/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    mov     r0, rINST, lsr #8           @ r0<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r0, r0, #15
    GET_VREG(r3, r1)                    @ r3<- vB
    GET_VREG(r2, r0)                    @ r2<- vA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, r3                      @ compare (vA, vB)
    moveq r1, #2                 @ r1<- BYTE branch dist for not-taken
    adds    r2, r1, r1                  @ convert to bytes, check sign
    FETCH_ADVANCE_INST_RB(r2)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
    cmp     r0,#0
    bne     common_updateProfile
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IF_LT: /* 0x34 */
/* File: armv5te/OP_IF_LT.S */
/* File: armv5te/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    mov     r0, rINST, lsr #8           @ r0<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r0, r0, #15
    GET_VREG(r3, r1)                    @ r3<- vB
    GET_VREG(r2, r0)                    @ r2<- vA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, r3                      @ compare (vA, vB)
    movge r1, #2                 @ r1<- BYTE branch dist for not-taken
    adds    r2, r1, r1                  @ convert to bytes, check sign
    FETCH_ADVANCE_INST_RB(r2)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
    cmp     r0,#0
    bne     common_updateProfile
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IF_GE: /* 0x35 */
/* File: armv5te/OP_IF_GE.S */
/* File: armv5te/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    mov     r0, rINST, lsr #8           @ r0<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r0, r0, #15
    GET_VREG(r3, r1)                    @ r3<- vB
    GET_VREG(r2, r0)                    @ r2<- vA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, r3                      @ compare (vA, vB)
    movlt r1, #2                 @ r1<- BYTE branch dist for not-taken
    adds    r2, r1, r1                  @ convert to bytes, check sign
    FETCH_ADVANCE_INST_RB(r2)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
    cmp     r0,#0
    bne     common_updateProfile
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IF_GT: /* 0x36 */
/* File: armv5te/OP_IF_GT.S */
/* File: armv5te/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    mov     r0, rINST, lsr #8           @ r0<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r0, r0, #15
    GET_VREG(r3, r1)                    @ r3<- vB
    GET_VREG(r2, r0)                    @ r2<- vA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, r3                      @ compare (vA, vB)
    movle r1, #2                 @ r1<- BYTE branch dist for not-taken
    adds    r2, r1, r1                  @ convert to bytes, check sign
    FETCH_ADVANCE_INST_RB(r2)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
    cmp     r0,#0
    bne     common_updateProfile
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IF_LE: /* 0x37 */
/* File: armv5te/OP_IF_LE.S */
/* File: armv5te/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    mov     r0, rINST, lsr #8           @ r0<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r0, r0, #15
    GET_VREG(r3, r1)                    @ r3<- vB
    GET_VREG(r2, r0)                    @ r2<- vA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, r3                      @ compare (vA, vB)
    movgt r1, #2                 @ r1<- BYTE branch dist for not-taken
    adds    r2, r1, r1                  @ convert to bytes, check sign
    FETCH_ADVANCE_INST_RB(r2)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
    cmp     r0,#0
    bne     common_updateProfile
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IF_EQZ: /* 0x38 */
/* File: armv5te/OP_IF_EQZ.S */
/* File: armv5te/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    mov     r0, rINST, lsr #8           @ r0<- AA
    GET_VREG(r2, r0)                    @ r2<- vAA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, #0                      @ compare (vA, 0)
    movne r1, #2                 @ r1<- inst branch dist for not-taken
    adds    r1, r1, r1                  @ convert to bytes & set flags
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]   @ refresh table base
    cmp     r0,#0
    bne     common_updateProfile        @ test for JIT off at target
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]   @ refresh table base
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IF_NEZ: /* 0x39 */
/* File: armv5te/OP_IF_NEZ.S */
/* File: armv5te/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    mov     r0, rINST, lsr #8           @ r0<- AA
    GET_VREG(r2, r0)                    @ r2<- vAA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, #0                      @ compare (vA, 0)
    moveq r1, #2                 @ r1<- inst branch dist for not-taken
    adds    r1, r1, r1                  @ convert to bytes & set flags
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]   @ refresh table base
    cmp     r0,#0
    bne     common_updateProfile        @ test for JIT off at target
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]   @ refresh table base
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IF_LTZ: /* 0x3a */
/* File: armv5te/OP_IF_LTZ.S */
/* File: armv5te/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    mov     r0, rINST, lsr #8           @ r0<- AA
    GET_VREG(r2, r0)                    @ r2<- vAA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, #0                      @ compare (vA, 0)
    movge r1, #2                 @ r1<- inst branch dist for not-taken
    adds    r1, r1, r1                  @ convert to bytes & set flags
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]   @ refresh table base
    cmp     r0,#0
    bne     common_updateProfile        @ test for JIT off at target
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]   @ refresh table base
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IF_GEZ: /* 0x3b */
/* File: armv5te/OP_IF_GEZ.S */
/* File: armv5te/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    mov     r0, rINST, lsr #8           @ r0<- AA
    GET_VREG(r2, r0)                    @ r2<- vAA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, #0                      @ compare (vA, 0)
    movlt r1, #2                 @ r1<- inst branch dist for not-taken
    adds    r1, r1, r1                  @ convert to bytes & set flags
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]   @ refresh table base
    cmp     r0,#0
    bne     common_updateProfile        @ test for JIT off at target
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]   @ refresh table base
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IF_GTZ: /* 0x3c */
/* File: armv5te/OP_IF_GTZ.S */
/* File: armv5te/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    mov     r0, rINST, lsr #8           @ r0<- AA
    GET_VREG(r2, r0)                    @ r2<- vAA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, #0                      @ compare (vA, 0)
    movle r1, #2                 @ r1<- inst branch dist for not-taken
    adds    r1, r1, r1                  @ convert to bytes & set flags
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]   @ refresh table base
    cmp     r0,#0
    bne     common_updateProfile        @ test for JIT off at target
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]   @ refresh table base
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IF_LEZ: /* 0x3d */
/* File: armv5te/OP_IF_LEZ.S */
/* File: armv5te/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    mov     r0, rINST, lsr #8           @ r0<- AA
    GET_VREG(r2, r0)                    @ r2<- vAA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, #0                      @ compare (vA, 0)
    movgt r1, #2                 @ r1<- inst branch dist for not-taken
    adds    r1, r1, r1                  @ convert to bytes & set flags
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]   @ refresh table base
    cmp     r0,#0
    bne     common_updateProfile        @ test for JIT off at target
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]   @ refresh table base
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_UNUSED_3E: /* 0x3e */
/* File: armv5te/OP_UNUSED_3E.S */
/* File: armv5te/unused.S */
    bl      common_abort


/* ------------------------------ */
    .balign 64
.L_OP_UNUSED_3F: /* 0x3f */
/* File: armv5te/OP_UNUSED_3F.S */
/* File: armv5te/unused.S */
    bl      common_abort


/* ------------------------------ */
    .balign 64
.L_OP_UNUSED_40: /* 0x40 */
/* File: armv5te/OP_UNUSED_40.S */
/* File: armv5te/unused.S */
    bl      common_abort


/* ------------------------------ */
    .balign 64
.L_OP_UNUSED_41: /* 0x41 */
/* File: armv5te/OP_UNUSED_41.S */
/* File: armv5te/unused.S */
    bl      common_abort


/* ------------------------------ */
    .balign 64
.L_OP_UNUSED_42: /* 0x42 */
/* File: armv5te/OP_UNUSED_42.S */
/* File: armv5te/unused.S */
    bl      common_abort


/* ------------------------------ */
    .balign 64
.L_OP_UNUSED_43: /* 0x43 */
/* File: armv5te/OP_UNUSED_43.S */
/* File: armv5te/unused.S */
    bl      common_abort


/* ------------------------------ */
    .balign 64
.L_OP_AGET: /* 0x44 */
/* File: armv5te/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(r2, 1, 0)                   @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B(r3, 1, 1)                   @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #2     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldr   r2, [r0, #offArrayObject_contents]  @ r2<- vBB[vCC]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r2, r9)                    @ vAA<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_AGET_WIDE: /* 0x45 */
/* File: armv5te/OP_AGET_WIDE.S */
    /*
     * Array get, 64 bits.  vAA <- vBB[vCC].
     *
     * Arrays of long/double are 64-bit aligned, so it's okay to use LDRD.
     */
    /* aget-wide vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #3          @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcc     .LOP_AGET_WIDE_finish          @ okay, continue below
    b       common_errArrayIndex        @ index >= length, bail
    @ May want to swap the order of these two branches depending on how the
    @ branch prediction (if any) handles conditional forward branches vs.
    @ unconditional forward branches.

/* ------------------------------ */
    .balign 64
.L_OP_AGET_OBJECT: /* 0x46 */
/* File: armv5te/OP_AGET_OBJECT.S */
/* File: armv5te/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(r2, 1, 0)                   @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B(r3, 1, 1)                   @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #2     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldr   r2, [r0, #offArrayObject_contents]  @ r2<- vBB[vCC]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r2, r9)                    @ vAA<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_AGET_BOOLEAN: /* 0x47 */
/* File: armv5te/OP_AGET_BOOLEAN.S */
/* File: armv5te/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(r2, 1, 0)                   @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B(r3, 1, 1)                   @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #0     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldrb   r2, [r0, #offArrayObject_contents]  @ r2<- vBB[vCC]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r2, r9)                    @ vAA<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_AGET_BYTE: /* 0x48 */
/* File: armv5te/OP_AGET_BYTE.S */
/* File: armv5te/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(r2, 1, 0)                   @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B(r3, 1, 1)                   @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #0     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldrsb   r2, [r0, #offArrayObject_contents]  @ r2<- vBB[vCC]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r2, r9)                    @ vAA<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_AGET_CHAR: /* 0x49 */
/* File: armv5te/OP_AGET_CHAR.S */
/* File: armv5te/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(r2, 1, 0)                   @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B(r3, 1, 1)                   @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #1     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldrh   r2, [r0, #offArrayObject_contents]  @ r2<- vBB[vCC]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r2, r9)                    @ vAA<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_AGET_SHORT: /* 0x4a */
/* File: armv5te/OP_AGET_SHORT.S */
/* File: armv5te/OP_AGET.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(r2, 1, 0)                   @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B(r3, 1, 1)                   @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #1     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldrsh   r2, [r0, #offArrayObject_contents]  @ r2<- vBB[vCC]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r2, r9)                    @ vAA<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_APUT: /* 0x4b */
/* File: armv5te/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(r2, 1, 0)                   @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B(r3, 1, 1)                   @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #2     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r2, r9)                    @ r2<- vAA
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    str  r2, [r0, #offArrayObject_contents]  @ vBB[vCC]<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_APUT_WIDE: /* 0x4c */
/* File: armv5te/OP_APUT_WIDE.S */
    /*
     * Array put, 64 bits.  vBB[vCC] <- vAA.
     *
     * Arrays of long/double are 64-bit aligned, so it's okay to use STRD.
     */
    /* aput-wide vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #3          @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    bcc     .LOP_APUT_WIDE_finish          @ okay, continue below
    b       common_errArrayIndex        @ index >= length, bail
    @ May want to swap the order of these two branches depending on how the
    @ branch prediction (if any) handles conditional forward branches vs.
    @ unconditional forward branches.

/* ------------------------------ */
    .balign 64
.L_OP_APUT_OBJECT: /* 0x4d */
/* File: armv5te/OP_APUT_OBJECT.S */
    /*
     * Store an object into an array.  vBB[vCC] <- vAA.
     */
    /* op vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    GET_VREG(rINST, r2)                 @ rINST<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     rINST, #0                   @ null array object?
    GET_VREG(r9, r9)                    @ r9<- vAA
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [rINST, #offArrayObject_length]   @ r3<- arrayObj->length
    add     r10, rINST, r1, lsl #2      @ r10<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcc     .LOP_APUT_OBJECT_finish          @ we're okay, continue on
    b       common_errArrayIndex        @ index >= length, bail


/* ------------------------------ */
    .balign 64
.L_OP_APUT_BOOLEAN: /* 0x4e */
/* File: armv5te/OP_APUT_BOOLEAN.S */
/* File: armv5te/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(r2, 1, 0)                   @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B(r3, 1, 1)                   @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #0     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r2, r9)                    @ r2<- vAA
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    strb  r2, [r0, #offArrayObject_contents]  @ vBB[vCC]<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_APUT_BYTE: /* 0x4f */
/* File: armv5te/OP_APUT_BYTE.S */
/* File: armv5te/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(r2, 1, 0)                   @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B(r3, 1, 1)                   @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #0     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r2, r9)                    @ r2<- vAA
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    strb  r2, [r0, #offArrayObject_contents]  @ vBB[vCC]<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_APUT_CHAR: /* 0x50 */
/* File: armv5te/OP_APUT_CHAR.S */
/* File: armv5te/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(r2, 1, 0)                   @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B(r3, 1, 1)                   @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #1     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r2, r9)                    @ r2<- vAA
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    strh  r2, [r0, #offArrayObject_contents]  @ vBB[vCC]<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_APUT_SHORT: /* 0x51 */
/* File: armv5te/OP_APUT_SHORT.S */
/* File: armv5te/OP_APUT.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(r2, 1, 0)                   @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B(r3, 1, 1)                   @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #1     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r2, r9)                    @ r2<- vAA
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    strh  r2, [r0, #offArrayObject_contents]  @ vBB[vCC]<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IGET: /* 0x52 */
/* File: armv5te/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IGET_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0
    bne     .LOP_IGET_finish
    b       common_exceptionThrown

/* ------------------------------ */
    .balign 64
.L_OP_IGET_WIDE: /* 0x53 */
/* File: armv5te/OP_IGET_WIDE.S */
    /*
     * Wide 32-bit instance field get.
     */
    /* iget-wide vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IGET_WIDE_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method] @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0
    bne     .LOP_IGET_WIDE_finish
    b       common_exceptionThrown

/* ------------------------------ */
    .balign 64
.L_OP_IGET_OBJECT: /* 0x54 */
/* File: armv5te/OP_IGET_OBJECT.S */
/* File: armv5te/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IGET_OBJECT_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0
    bne     .LOP_IGET_OBJECT_finish
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_IGET_BOOLEAN: /* 0x55 */
/* File: armv5te/OP_IGET_BOOLEAN.S */
@include "armv5te/OP_IGET.S" { "load":"ldrb", "sqnum":"1" }
/* File: armv5te/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IGET_BOOLEAN_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0
    bne     .LOP_IGET_BOOLEAN_finish
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_IGET_BYTE: /* 0x56 */
/* File: armv5te/OP_IGET_BYTE.S */
@include "armv5te/OP_IGET.S" { "load":"ldrsb", "sqnum":"2" }
/* File: armv5te/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IGET_BYTE_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0
    bne     .LOP_IGET_BYTE_finish
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_IGET_CHAR: /* 0x57 */
/* File: armv5te/OP_IGET_CHAR.S */
@include "armv5te/OP_IGET.S" { "load":"ldrh", "sqnum":"3" }
/* File: armv5te/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IGET_CHAR_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0
    bne     .LOP_IGET_CHAR_finish
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_IGET_SHORT: /* 0x58 */
/* File: armv5te/OP_IGET_SHORT.S */
@include "armv5te/OP_IGET.S" { "load":"ldrsh", "sqnum":"4" }
/* File: armv5te/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IGET_SHORT_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0
    bne     .LOP_IGET_SHORT_finish
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_IPUT: /* 0x59 */
/* File: armv5te/OP_IPUT.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IPUT_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ success?
    bne     .LOP_IPUT_finish          @ yes, finish up
    b       common_exceptionThrown

/* ------------------------------ */
    .balign 64
.L_OP_IPUT_WIDE: /* 0x5a */
/* File: armv5te/OP_IPUT_WIDE.S */
    /* iput-wide vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IPUT_WIDE_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method] @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ success?
    bne     .LOP_IPUT_WIDE_finish          @ yes, finish up
    b       common_exceptionThrown

/* ------------------------------ */
    .balign 64
.L_OP_IPUT_OBJECT: /* 0x5b */
/* File: armv5te/OP_IPUT_OBJECT.S */
    /*
     * 32-bit instance field put.
     *
     * for: iput-object, iput-object-volatile
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IPUT_OBJECT_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ success?
    bne     .LOP_IPUT_OBJECT_finish          @ yes, finish up
    b       common_exceptionThrown

/* ------------------------------ */
    .balign 64
.L_OP_IPUT_BOOLEAN: /* 0x5c */
/* File: armv5te/OP_IPUT_BOOLEAN.S */
@include "armv5te/OP_IPUT.S" { "store":"strb", "sqnum":"1" }
/* File: armv5te/OP_IPUT.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IPUT_BOOLEAN_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ success?
    bne     .LOP_IPUT_BOOLEAN_finish          @ yes, finish up
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_IPUT_BYTE: /* 0x5d */
/* File: armv5te/OP_IPUT_BYTE.S */
@include "armv5te/OP_IPUT.S" { "store":"strb", "sqnum":"2" }
/* File: armv5te/OP_IPUT.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IPUT_BYTE_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ success?
    bne     .LOP_IPUT_BYTE_finish          @ yes, finish up
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_IPUT_CHAR: /* 0x5e */
/* File: armv5te/OP_IPUT_CHAR.S */
@include "armv5te/OP_IPUT.S" { "store":"strh", "sqnum":"3" }
/* File: armv5te/OP_IPUT.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IPUT_CHAR_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ success?
    bne     .LOP_IPUT_CHAR_finish          @ yes, finish up
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_IPUT_SHORT: /* 0x5f */
/* File: armv5te/OP_IPUT_SHORT.S */
@include "armv5te/OP_IPUT.S" { "store":"strh", "sqnum":"4" }
/* File: armv5te/OP_IPUT.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IPUT_SHORT_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ success?
    bne     .LOP_IPUT_SHORT_finish          @ yes, finish up
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_SGET: /* 0x60 */
/* File: armv5te/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SGET_resolve         @ yes, do resolve
.LOP_SGET_finish: @ field ptr in r0
    ldr     r1, [r0, #offStaticField_value] @ r1<- field value
    @ no-op                             @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r2)                    @ fp[AA]<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_SGET_WIDE: /* 0x61 */
/* File: armv5te/OP_SGET_WIDE.S */
    /*
     * 64-bit SGET handler.
     */
    /* sget-wide vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SGET_WIDE_resolve         @ yes, do resolve
.LOP_SGET_WIDE_finish:
    mov     r9, rINST, lsr #8           @ r9<- AA
    .if 0
    add     r0, r0, #offStaticField_value @ r0<- pointer to data
    bl      dvmQuasiAtomicRead64        @ r0/r1<- contents of field
    .else
    ldrd    r0, [r0, #offStaticField_value] @ r0/r1<- field value (aligned)
    .endif
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    stmia   r9, {r0-r1}                 @ vAA/vAA+1<- r0/r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_SGET_OBJECT: /* 0x62 */
/* File: armv5te/OP_SGET_OBJECT.S */
/* File: armv5te/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SGET_OBJECT_resolve         @ yes, do resolve
.LOP_SGET_OBJECT_finish: @ field ptr in r0
    ldr     r1, [r0, #offStaticField_value] @ r1<- field value
    @ no-op                             @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r2)                    @ fp[AA]<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SGET_BOOLEAN: /* 0x63 */
/* File: armv5te/OP_SGET_BOOLEAN.S */
/* File: armv5te/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SGET_BOOLEAN_resolve         @ yes, do resolve
.LOP_SGET_BOOLEAN_finish: @ field ptr in r0
    ldr     r1, [r0, #offStaticField_value] @ r1<- field value
    @ no-op                             @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r2)                    @ fp[AA]<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SGET_BYTE: /* 0x64 */
/* File: armv5te/OP_SGET_BYTE.S */
/* File: armv5te/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SGET_BYTE_resolve         @ yes, do resolve
.LOP_SGET_BYTE_finish: @ field ptr in r0
    ldr     r1, [r0, #offStaticField_value] @ r1<- field value
    @ no-op                             @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r2)                    @ fp[AA]<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SGET_CHAR: /* 0x65 */
/* File: armv5te/OP_SGET_CHAR.S */
/* File: armv5te/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SGET_CHAR_resolve         @ yes, do resolve
.LOP_SGET_CHAR_finish: @ field ptr in r0
    ldr     r1, [r0, #offStaticField_value] @ r1<- field value
    @ no-op                             @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r2)                    @ fp[AA]<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SGET_SHORT: /* 0x66 */
/* File: armv5te/OP_SGET_SHORT.S */
/* File: armv5te/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SGET_SHORT_resolve         @ yes, do resolve
.LOP_SGET_SHORT_finish: @ field ptr in r0
    ldr     r1, [r0, #offStaticField_value] @ r1<- field value
    @ no-op                             @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r2)                    @ fp[AA]<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SPUT: /* 0x67 */
/* File: armv5te/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]        @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SPUT_resolve         @ yes, do resolve
.LOP_SPUT_finish:   @ field ptr in r0
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r1, r2)                    @ r1<- fp[AA]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    @ no-op                        @ releasing store
    str     r1, [r0, #offStaticField_value] @ field<- vAA
    @ no-op 
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_SPUT_WIDE: /* 0x68 */
/* File: armv5te/OP_SPUT_WIDE.S */
    /*
     * 64-bit SPUT handler.
     */
    /* sput-wide vAA, field@BBBB */
    ldr     r0, [rSELF, #offThread_methodClassDex]  @ r0<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r0, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    mov     r9, rINST, lsr #8           @ r9<- AA
    ldr     r2, [r10, r1, lsl #2]        @ r2<- resolved StaticField ptr
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    cmp     r2, #0                      @ is resolved entry null?
    beq     .LOP_SPUT_WIDE_resolve         @ yes, do resolve
.LOP_SPUT_WIDE_finish: @ field ptr in r2, AA in r9
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1
    GET_INST_OPCODE(r10)                @ extract opcode from rINST
    .if 0
    add     r2, r2, #offStaticField_value @ r2<- pointer to data
    bl      dvmQuasiAtomicSwap64Sync    @ stores r0/r1 into addr r2
    .else
    strd    r0, [r2, #offStaticField_value] @ field<- vAA/vAA+1
    .endif
    GOTO_OPCODE(r10)                    @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_SPUT_OBJECT: /* 0x69 */
/* File: armv5te/OP_SPUT_OBJECT.S */
    /*
     * 32-bit SPUT handler for objects
     *
     * for: sput-object, sput-object-volatile
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]        @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SPUT_OBJECT_resolve         @ yes, do resolve
.LOP_SPUT_OBJECT_finish:   @ field ptr in r0
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r1, r2)                    @ r1<- fp[AA]
    ldr     r2, [rSELF, #offThread_cardTable]  @ r2<- card table base
    ldr     r9, [r0, #offField_clazz]   @ r9<- field->clazz
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    @ no-op                         @ releasing store
    b       .LOP_SPUT_OBJECT_end

/* ------------------------------ */
    .balign 64
.L_OP_SPUT_BOOLEAN: /* 0x6a */
/* File: armv5te/OP_SPUT_BOOLEAN.S */
/* File: armv5te/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]        @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SPUT_BOOLEAN_resolve         @ yes, do resolve
.LOP_SPUT_BOOLEAN_finish:   @ field ptr in r0
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r1, r2)                    @ r1<- fp[AA]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    @ no-op                        @ releasing store
    str     r1, [r0, #offStaticField_value] @ field<- vAA
    @ no-op 
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SPUT_BYTE: /* 0x6b */
/* File: armv5te/OP_SPUT_BYTE.S */
/* File: armv5te/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]        @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SPUT_BYTE_resolve         @ yes, do resolve
.LOP_SPUT_BYTE_finish:   @ field ptr in r0
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r1, r2)                    @ r1<- fp[AA]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    @ no-op                        @ releasing store
    str     r1, [r0, #offStaticField_value] @ field<- vAA
    @ no-op 
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SPUT_CHAR: /* 0x6c */
/* File: armv5te/OP_SPUT_CHAR.S */
/* File: armv5te/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]        @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SPUT_CHAR_resolve         @ yes, do resolve
.LOP_SPUT_CHAR_finish:   @ field ptr in r0
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r1, r2)                    @ r1<- fp[AA]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    @ no-op                        @ releasing store
    str     r1, [r0, #offStaticField_value] @ field<- vAA
    @ no-op 
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SPUT_SHORT: /* 0x6d */
/* File: armv5te/OP_SPUT_SHORT.S */
/* File: armv5te/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]        @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SPUT_SHORT_resolve         @ yes, do resolve
.LOP_SPUT_SHORT_finish:   @ field ptr in r0
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r1, r2)                    @ r1<- fp[AA]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    @ no-op                        @ releasing store
    str     r1, [r0, #offStaticField_value] @ field<- vAA
    @ no-op 
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_VIRTUAL: /* 0x6e */
/* File: armv5te/OP_INVOKE_VIRTUAL.S */
    /*
     * Handle a virtual method call.
     *
     * for: invoke-virtual, invoke-virtual/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResMethods]    @ r3<- pDvmDex->pResMethods
    FETCH(r10, 2)                       @ r10<- GFED or CCCC
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved baseMethod
    .if     (!0)
    and     r10, r10, #15               @ r10<- D (or stays CCCC)
    .endif
    cmp     r0, #0                      @ already resolved?
    EXPORT_PC()                         @ must export for invoke
    bne     .LOP_INVOKE_VIRTUAL_continue        @ yes, continue on
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    mov     r2, #METHOD_VIRTUAL         @ resolver method type
    bl      dvmResolveMethod            @ r0<- call(clazz, ref, flags)
    cmp     r0, #0                      @ got null?
    bne     .LOP_INVOKE_VIRTUAL_continue        @ no, continue
    b       common_exceptionThrown      @ yes, handle exception

/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_SUPER: /* 0x6f */
/* File: armv5te/OP_INVOKE_SUPER.S */
    /*
     * Handle a "super" method call.
     *
     * for: invoke-super, invoke-super/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(r10, 2)                       @ r10<- GFED or CCCC
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    .if     (!0)
    and     r10, r10, #15               @ r10<- D (or stays CCCC)
    .endif
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResMethods]    @ r3<- pDvmDex->pResMethods
    GET_VREG(r9, r10)                   @ r9<- "this" ptr
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved baseMethod
    cmp     r9, #0                      @ null "this"?
    ldr     r10, [rSELF, #offThread_method] @ r10<- current method
    beq     common_errNullObject        @ null "this", throw exception
    cmp     r0, #0                      @ already resolved?
    ldr     r10, [r10, #offMethod_clazz]  @ r10<- method->clazz
    EXPORT_PC()                         @ must export for invoke
    bne     .LOP_INVOKE_SUPER_continue        @ resolved, continue on
    b       .LOP_INVOKE_SUPER_resolve         @ do resolve now

/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_DIRECT: /* 0x70 */
/* File: armv5te/OP_INVOKE_DIRECT.S */
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
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResMethods]    @ r3<- pDvmDex->pResMethods
    FETCH(r10, 2)                       @ r10<- GFED or CCCC
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved methodToCall
    .if     (!0)
    and     r10, r10, #15               @ r10<- D (or stays CCCC)
    .endif
    cmp     r0, #0                      @ already resolved?
    EXPORT_PC()                         @ must export for invoke
    GET_VREG(r9, r10)                   @ r9<- "this" ptr
    beq     .LOP_INVOKE_DIRECT_resolve         @ not resolved, do it now
.LOP_INVOKE_DIRECT_finish:
    cmp     r9, #0                      @ null "this" ref?
    bne     common_invokeMethodNoRange   @ r0=method, r9="this"
    b       common_errNullObject        @ yes, throw exception

/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_STATIC: /* 0x71 */
/* File: armv5te/OP_INVOKE_STATIC.S */
    /*
     * Handle a static method call.
     *
     * for: invoke-static, invoke-static/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResMethods]    @ r3<- pDvmDex->pResMethods
    mov     r9, #0                      @ null "this" in delay slot
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved methodToCall
#if defined(WITH_JIT)
    add     r10, r3, r1, lsl #2         @ r10<- &resolved_methodToCall
#endif
    cmp     r0, #0                      @ already resolved?
    EXPORT_PC()                         @ must export for invoke
    bne     common_invokeMethodNoRange @ yes, continue on
    b       .LOP_INVOKE_STATIC_resolve

/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_INTERFACE: /* 0x72 */
/* File: armv5te/OP_INVOKE_INTERFACE.S */
    /*
     * Handle an interface method call.
     *
     * for: invoke-interface, invoke-interface/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(r2, 2)                        @ r2<- FEDC or CCCC
    FETCH(r1, 1)                        @ r1<- BBBB
    .if     (!0)
    and     r2, r2, #15                 @ r2<- C (or stays CCCC)
    .endif
    EXPORT_PC()                         @ must export for invoke
    GET_VREG(r9, r2)                    @ r9<- first arg ("this")
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- methodClassDex
    cmp     r9, #0                      @ null obj?
    ldr     r2, [rSELF, #offThread_method]  @ r2<- method
    beq     common_errNullObject        @ yes, fail
    ldr     r0, [r9, #offObject_clazz]  @ r0<- thisPtr->clazz
    bl      dvmFindInterfaceMethodInCache @ r0<- call(class, ref, method, dex)
    cmp     r0, #0                      @ failed?
    beq     common_exceptionThrown      @ yes, handle exception
    b       common_invokeMethodNoRange @ (r0=method, r9="this")

/* ------------------------------ */
    .balign 64
.L_OP_UNUSED_73: /* 0x73 */
/* File: armv5te/OP_UNUSED_73.S */
/* File: armv5te/unused.S */
    bl      common_abort


/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_VIRTUAL_RANGE: /* 0x74 */
/* File: armv5te/OP_INVOKE_VIRTUAL_RANGE.S */
/* File: armv5te/OP_INVOKE_VIRTUAL.S */
    /*
     * Handle a virtual method call.
     *
     * for: invoke-virtual, invoke-virtual/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResMethods]    @ r3<- pDvmDex->pResMethods
    FETCH(r10, 2)                       @ r10<- GFED or CCCC
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved baseMethod
    .if     (!1)
    and     r10, r10, #15               @ r10<- D (or stays CCCC)
    .endif
    cmp     r0, #0                      @ already resolved?
    EXPORT_PC()                         @ must export for invoke
    bne     .LOP_INVOKE_VIRTUAL_RANGE_continue        @ yes, continue on
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    mov     r2, #METHOD_VIRTUAL         @ resolver method type
    bl      dvmResolveMethod            @ r0<- call(clazz, ref, flags)
    cmp     r0, #0                      @ got null?
    bne     .LOP_INVOKE_VIRTUAL_RANGE_continue        @ no, continue
    b       common_exceptionThrown      @ yes, handle exception


/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_SUPER_RANGE: /* 0x75 */
/* File: armv5te/OP_INVOKE_SUPER_RANGE.S */
/* File: armv5te/OP_INVOKE_SUPER.S */
    /*
     * Handle a "super" method call.
     *
     * for: invoke-super, invoke-super/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(r10, 2)                       @ r10<- GFED or CCCC
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    .if     (!1)
    and     r10, r10, #15               @ r10<- D (or stays CCCC)
    .endif
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResMethods]    @ r3<- pDvmDex->pResMethods
    GET_VREG(r9, r10)                   @ r9<- "this" ptr
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved baseMethod
    cmp     r9, #0                      @ null "this"?
    ldr     r10, [rSELF, #offThread_method] @ r10<- current method
    beq     common_errNullObject        @ null "this", throw exception
    cmp     r0, #0                      @ already resolved?
    ldr     r10, [r10, #offMethod_clazz]  @ r10<- method->clazz
    EXPORT_PC()                         @ must export for invoke
    bne     .LOP_INVOKE_SUPER_RANGE_continue        @ resolved, continue on
    b       .LOP_INVOKE_SUPER_RANGE_resolve         @ do resolve now


/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_DIRECT_RANGE: /* 0x76 */
/* File: armv5te/OP_INVOKE_DIRECT_RANGE.S */
/* File: armv5te/OP_INVOKE_DIRECT.S */
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
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResMethods]    @ r3<- pDvmDex->pResMethods
    FETCH(r10, 2)                       @ r10<- GFED or CCCC
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved methodToCall
    .if     (!1)
    and     r10, r10, #15               @ r10<- D (or stays CCCC)
    .endif
    cmp     r0, #0                      @ already resolved?
    EXPORT_PC()                         @ must export for invoke
    GET_VREG(r9, r10)                   @ r9<- "this" ptr
    beq     .LOP_INVOKE_DIRECT_RANGE_resolve         @ not resolved, do it now
.LOP_INVOKE_DIRECT_RANGE_finish:
    cmp     r9, #0                      @ null "this" ref?
    bne     common_invokeMethodRange   @ r0=method, r9="this"
    b       common_errNullObject        @ yes, throw exception


/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_STATIC_RANGE: /* 0x77 */
/* File: armv5te/OP_INVOKE_STATIC_RANGE.S */
/* File: armv5te/OP_INVOKE_STATIC.S */
    /*
     * Handle a static method call.
     *
     * for: invoke-static, invoke-static/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResMethods]    @ r3<- pDvmDex->pResMethods
    mov     r9, #0                      @ null "this" in delay slot
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved methodToCall
#if defined(WITH_JIT)
    add     r10, r3, r1, lsl #2         @ r10<- &resolved_methodToCall
#endif
    cmp     r0, #0                      @ already resolved?
    EXPORT_PC()                         @ must export for invoke
    bne     common_invokeMethodRange @ yes, continue on
    b       .LOP_INVOKE_STATIC_RANGE_resolve


/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_INTERFACE_RANGE: /* 0x78 */
/* File: armv5te/OP_INVOKE_INTERFACE_RANGE.S */
/* File: armv5te/OP_INVOKE_INTERFACE.S */
    /*
     * Handle an interface method call.
     *
     * for: invoke-interface, invoke-interface/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(r2, 2)                        @ r2<- FEDC or CCCC
    FETCH(r1, 1)                        @ r1<- BBBB
    .if     (!1)
    and     r2, r2, #15                 @ r2<- C (or stays CCCC)
    .endif
    EXPORT_PC()                         @ must export for invoke
    GET_VREG(r9, r2)                    @ r9<- first arg ("this")
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- methodClassDex
    cmp     r9, #0                      @ null obj?
    ldr     r2, [rSELF, #offThread_method]  @ r2<- method
    beq     common_errNullObject        @ yes, fail
    ldr     r0, [r9, #offObject_clazz]  @ r0<- thisPtr->clazz
    bl      dvmFindInterfaceMethodInCache @ r0<- call(class, ref, method, dex)
    cmp     r0, #0                      @ failed?
    beq     common_exceptionThrown      @ yes, handle exception
    b       common_invokeMethodRange @ (r0=method, r9="this")


/* ------------------------------ */
    .balign 64
.L_OP_UNUSED_79: /* 0x79 */
/* File: armv5te/OP_UNUSED_79.S */
/* File: armv5te/unused.S */
    bl      common_abort


/* ------------------------------ */
    .balign 64
.L_OP_UNUSED_7A: /* 0x7a */
/* File: armv5te/OP_UNUSED_7A.S */
/* File: armv5te/unused.S */
    bl      common_abort


/* ------------------------------ */
    .balign 64
.L_OP_NEG_INT: /* 0x7b */
/* File: armv5te/OP_NEG_INT.S */
/* File: armv5te/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0".
     * This could be an ARM instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r3)                    @ r0<- vB
    and     r9, r9, #15
                               @ optional op; may set condition codes
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    rsb     r0, r0, #0                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_NOT_INT: /* 0x7c */
/* File: armv5te/OP_NOT_INT.S */
/* File: armv5te/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0".
     * This could be an ARM instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r3)                    @ r0<- vB
    and     r9, r9, #15
                               @ optional op; may set condition codes
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    mvn     r0, r0                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_NEG_LONG: /* 0x7d */
/* File: armv5te/OP_NEG_LONG.S */
/* File: armv5te/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0/r1".
     * This could be an ARM instruction or a function call.
     *
     * For: neg-long, not-long, neg-double, long-to-double, double-to-long
     */
    /* unop vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r3, {r0-r1}                 @ r0/r1<- vAA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    rsbs    r0, r0, #0                           @ optional op; may set condition codes
    rsc     r1, r1, #0                              @ r0/r1<- op, r2-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_NOT_LONG: /* 0x7e */
/* File: armv5te/OP_NOT_LONG.S */
/* File: armv5te/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0/r1".
     * This could be an ARM instruction or a function call.
     *
     * For: neg-long, not-long, neg-double, long-to-double, double-to-long
     */
    /* unop vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r3, {r0-r1}                 @ r0/r1<- vAA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    mvn     r0, r0                           @ optional op; may set condition codes
    mvn     r1, r1                              @ r0/r1<- op, r2-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_NEG_FLOAT: /* 0x7f */
/* File: armv5te/OP_NEG_FLOAT.S */
/* File: armv5te/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0".
     * This could be an ARM instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r3)                    @ r0<- vB
    and     r9, r9, #15
                               @ optional op; may set condition codes
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    add     r0, r0, #0x80000000                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_NEG_DOUBLE: /* 0x80 */
/* File: armv5te/OP_NEG_DOUBLE.S */
/* File: armv5te/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0/r1".
     * This could be an ARM instruction or a function call.
     *
     * For: neg-long, not-long, neg-double, long-to-double, double-to-long
     */
    /* unop vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r3, {r0-r1}                 @ r0/r1<- vAA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    add     r1, r1, #0x80000000                              @ r0/r1<- op, r2-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_INT_TO_LONG: /* 0x81 */
/* File: armv5te/OP_INT_TO_LONG.S */
/* File: armv5te/unopWider.S */
    /*
     * Generic 32bit-to-64bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op r0", where
     * "result" is a 64-bit quantity in r0/r1.
     *
     * For: int-to-long, int-to-double, float-to-long, float-to-double
     */
    /* unop vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r0, r3)                    @ r0<- vB
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
                               @ optional op; may set condition codes
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    mov     r1, r0, asr #31                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vA/vA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-11 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_INT_TO_FLOAT: /* 0x82 */
/* File: arm-vfp/OP_INT_TO_FLOAT.S */
/* File: arm-vfp/funop.S */
    /*
     * Generic 32-bit unary floating-point operation.  Provide an "instr"
     * line that specifies an instruction that performs "s1 = op s0".
     *
     * for: int-to-float, float-to-int
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    flds    s0, [r3]                    @ s0<- vB
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    and     r9, r9, #15                 @ r9<- A
    fsitos  s1, s0                              @ s1<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    fsts    s1, [r9]                    @ vA<- s1
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_INT_TO_DOUBLE: /* 0x83 */
/* File: arm-vfp/OP_INT_TO_DOUBLE.S */
/* File: arm-vfp/funopWider.S */
    /*
     * Generic 32bit-to-64bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "d0 = op s0".
     *
     * For: int-to-double, float-to-double
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    flds    s0, [r3]                    @ s0<- vB
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    and     r9, r9, #15                 @ r9<- A
    fsitod  d0, s0                              @ d0<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    fstd    d0, [r9]                    @ vA<- d0
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_LONG_TO_INT: /* 0x84 */
/* File: armv5te/OP_LONG_TO_INT.S */
/* we ignore the high word, making this equivalent to a 32-bit reg move */
/* File: armv5te/OP_MOVE.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    mov     r1, rINST, lsr #12          @ r1<- B from 15:12
    mov     r0, rINST, lsr #8           @ r0<- A from 11:8
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    GET_VREG(r2, r1)                    @ r2<- fp[B]
    and     r0, r0, #15
    GET_INST_OPCODE(ip)                 @ ip<- opcode from rINST
    SET_VREG(r2, r0)                    @ fp[A]<- r2
    GOTO_OPCODE(ip)                     @ execute next instruction


/* ------------------------------ */
    .balign 64
.L_OP_LONG_TO_FLOAT: /* 0x85 */
/* File: armv5te/OP_LONG_TO_FLOAT.S */
/* File: armv5te/unopNarrower.S */
    /*
     * Generic 64bit-to-32bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op r0/r1", where
     * "result" is a 32-bit quantity in r0.
     *
     * For: long-to-float, double-to-int, double-to-float
     *
     * (This would work for long-to-int, but that instruction is actually
     * an exact match for OP_MOVE.)
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[B]
    and     r9, r9, #15
    ldmia   r3, {r0-r1}                 @ r0/r1<- vB/vB+1
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    bl      __aeabi_l2f                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)                    @ vA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-11 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_LONG_TO_DOUBLE: /* 0x86 */
/* File: armv5te/OP_LONG_TO_DOUBLE.S */
/* File: armv5te/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0/r1".
     * This could be an ARM instruction or a function call.
     *
     * For: neg-long, not-long, neg-double, long-to-double, double-to-long
     */
    /* unop vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r3, {r0-r1}                 @ r0/r1<- vAA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    bl      __aeabi_l2d                              @ r0/r1<- op, r2-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_FLOAT_TO_INT: /* 0x87 */
/* File: arm-vfp/OP_FLOAT_TO_INT.S */
/* File: arm-vfp/funop.S */
    /*
     * Generic 32-bit unary floating-point operation.  Provide an "instr"
     * line that specifies an instruction that performs "s1 = op s0".
     *
     * for: int-to-float, float-to-int
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    flds    s0, [r3]                    @ s0<- vB
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    and     r9, r9, #15                 @ r9<- A
    ftosizs s1, s0                              @ s1<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    fsts    s1, [r9]                    @ vA<- s1
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_FLOAT_TO_LONG: /* 0x88 */
/* File: armv5te/OP_FLOAT_TO_LONG.S */
@include "armv5te/unopWider.S" {"instr":"bl      __aeabi_f2lz"}
/* File: armv5te/unopWider.S */
    /*
     * Generic 32bit-to-64bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op r0", where
     * "result" is a 64-bit quantity in r0/r1.
     *
     * For: int-to-long, int-to-double, float-to-long, float-to-double
     */
    /* unop vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r0, r3)                    @ r0<- vB
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
                               @ optional op; may set condition codes
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    bl      f2l_doconv                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vA/vA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-11 instructions */



/* ------------------------------ */
    .balign 64
.L_OP_FLOAT_TO_DOUBLE: /* 0x89 */
/* File: arm-vfp/OP_FLOAT_TO_DOUBLE.S */
/* File: arm-vfp/funopWider.S */
    /*
     * Generic 32bit-to-64bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "d0 = op s0".
     *
     * For: int-to-double, float-to-double
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    flds    s0, [r3]                    @ s0<- vB
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    and     r9, r9, #15                 @ r9<- A
    fcvtds  d0, s0                              @ d0<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    fstd    d0, [r9]                    @ vA<- d0
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_DOUBLE_TO_INT: /* 0x8a */
/* File: arm-vfp/OP_DOUBLE_TO_INT.S */
/* File: arm-vfp/funopNarrower.S */
    /*
     * Generic 64bit-to-32bit unary floating point operation.  Provide an
     * "instr" line that specifies an instruction that performs "s0 = op d0".
     *
     * For: double-to-int, double-to-float
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    fldd    d0, [r3]                    @ d0<- vB
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    and     r9, r9, #15                 @ r9<- A
    ftosizd  s0, d0                              @ s0<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    fsts    s0, [r9]                    @ vA<- s0
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_DOUBLE_TO_LONG: /* 0x8b */
/* File: armv5te/OP_DOUBLE_TO_LONG.S */
@include "armv5te/unopWide.S" {"instr":"bl      __aeabi_d2lz"}
/* File: armv5te/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0/r1".
     * This could be an ARM instruction or a function call.
     *
     * For: neg-long, not-long, neg-double, long-to-double, double-to-long
     */
    /* unop vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r3, {r0-r1}                 @ r0/r1<- vAA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    bl      d2l_doconv                              @ r0/r1<- op, r2-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-13 instructions */



/* ------------------------------ */
    .balign 64
.L_OP_DOUBLE_TO_FLOAT: /* 0x8c */
/* File: arm-vfp/OP_DOUBLE_TO_FLOAT.S */
/* File: arm-vfp/funopNarrower.S */
    /*
     * Generic 64bit-to-32bit unary floating point operation.  Provide an
     * "instr" line that specifies an instruction that performs "s0 = op d0".
     *
     * For: double-to-int, double-to-float
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    fldd    d0, [r3]                    @ d0<- vB
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    and     r9, r9, #15                 @ r9<- A
    fcvtsd  s0, d0                              @ s0<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    fsts    s0, [r9]                    @ vA<- s0
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_INT_TO_BYTE: /* 0x8d */
/* File: armv5te/OP_INT_TO_BYTE.S */
/* File: armv5te/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0".
     * This could be an ARM instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r3)                    @ r0<- vB
    and     r9, r9, #15
    mov     r0, r0, asl #24                           @ optional op; may set condition codes
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    mov     r0, r0, asr #24                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_INT_TO_CHAR: /* 0x8e */
/* File: armv5te/OP_INT_TO_CHAR.S */
/* File: armv5te/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0".
     * This could be an ARM instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r3)                    @ r0<- vB
    and     r9, r9, #15
    mov     r0, r0, asl #16                           @ optional op; may set condition codes
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    mov     r0, r0, lsr #16                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_INT_TO_SHORT: /* 0x8f */
/* File: armv5te/OP_INT_TO_SHORT.S */
/* File: armv5te/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0".
     * This could be an ARM instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r3)                    @ r0<- vB
    and     r9, r9, #15
    mov     r0, r0, asl #16                           @ optional op; may set condition codes
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    mov     r0, r0, asr #16                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 9-10 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_ADD_INT: /* 0x90 */
/* File: armv5te/OP_ADD_INT.S */
/* File: armv5te/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    add     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_SUB_INT: /* 0x91 */
/* File: armv5te/OP_SUB_INT.S */
/* File: armv5te/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    sub     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_MUL_INT: /* 0x92 */
/* File: armv5te/OP_MUL_INT.S */
/* must be "mul r0, r1, r0" -- "r0, r0, r1" is illegal */
/* File: armv5te/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    mul     r0, r1, r0                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_DIV_INT: /* 0x93 */
/* File: armv5te/OP_DIV_INT.S */
/* File: armv5te/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if 1
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    bl     __aeabi_idiv                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_REM_INT: /* 0x94 */
/* File: armv5te/OP_REM_INT.S */
/* idivmod returns quotient in r0 and remainder in r1 */
/* File: armv5te/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if 1
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    bl      __aeabi_idivmod                              @ r1<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r1, r9)               @ vAA<- r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_AND_INT: /* 0x95 */
/* File: armv5te/OP_AND_INT.S */
/* File: armv5te/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    and     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_OR_INT: /* 0x96 */
/* File: armv5te/OP_OR_INT.S */
/* File: armv5te/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    orr     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_XOR_INT: /* 0x97 */
/* File: armv5te/OP_XOR_INT.S */
/* File: armv5te/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    eor     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_SHL_INT: /* 0x98 */
/* File: armv5te/OP_SHL_INT.S */
/* File: armv5te/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r1, r1, #31                           @ optional op; may set condition codes
    mov     r0, r0, asl r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_SHR_INT: /* 0x99 */
/* File: armv5te/OP_SHR_INT.S */
/* File: armv5te/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r1, r1, #31                           @ optional op; may set condition codes
    mov     r0, r0, asr r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_USHR_INT: /* 0x9a */
/* File: armv5te/OP_USHR_INT.S */
/* File: armv5te/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r1, r1, #31                           @ optional op; may set condition codes
    mov     r0, r0, lsr r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_ADD_LONG: /* 0x9b */
/* File: armv5te/OP_ADD_LONG.S */
/* File: armv5te/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[BB]
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[CC]
    ldmia   r2, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r3, {r2-r3}                 @ r2/r3<- vCC/vCC+1
    .if 0
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    adds    r0, r0, r2                           @ optional op; may set condition codes
    adc     r1, r1, r3                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 14-17 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_SUB_LONG: /* 0x9c */
/* File: armv5te/OP_SUB_LONG.S */
/* File: armv5te/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[BB]
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[CC]
    ldmia   r2, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r3, {r2-r3}                 @ r2/r3<- vCC/vCC+1
    .if 0
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    subs    r0, r0, r2                           @ optional op; may set condition codes
    sbc     r1, r1, r3                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 14-17 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_MUL_LONG: /* 0x9d */
/* File: armv5te/OP_MUL_LONG.S */
    /*
     * Signed 64-bit integer multiply.
     *
     * Consider WXxYZ (r1r0 x r3r2) with a long multiply:
     *        WX
     *      x YZ
     *  --------
     *     ZW ZX
     *  YW YX
     *
     * The low word of the result holds ZX, the high word holds
     * (ZW+YX) + (the high overflow from ZX).  YW doesn't matter because
     * it doesn't fit in the low 64 bits.
     *
     * Unlike most ARM math operations, multiply instructions have
     * restrictions on using the same register more than once (Rd and Rm
     * cannot be the same).
     */
    /* mul-long vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[BB]
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[CC]
    ldmia   r2, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r3, {r2-r3}                 @ r2/r3<- vCC/vCC+1
    mul     ip, r2, r1                  @  ip<- ZxW
    umull   r9, r10, r2, r0             @  r9/r10 <- ZxX
    mla     r2, r0, r3, ip              @  r2<- YxX + (ZxW)
    mov     r0, rINST, lsr #8           @ r0<- AA
    add     r10, r2, r10                @  r10<- r10 + low(ZxW + (YxX))
    add     r0, rFP, r0, lsl #2         @ r0<- &fp[AA]
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    b       .LOP_MUL_LONG_finish

/* ------------------------------ */
    .balign 64
.L_OP_DIV_LONG: /* 0x9e */
/* File: armv5te/OP_DIV_LONG.S */
/* File: armv5te/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[BB]
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[CC]
    ldmia   r2, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r3, {r2-r3}                 @ r2/r3<- vCC/vCC+1
    .if 1
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    bl      __aeabi_ldivmod                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 14-17 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_REM_LONG: /* 0x9f */
/* File: armv5te/OP_REM_LONG.S */
/* ldivmod returns quotient in r0/r1 and remainder in r2/r3 */
/* File: armv5te/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[BB]
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[CC]
    ldmia   r2, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r3, {r2-r3}                 @ r2/r3<- vCC/vCC+1
    .if 1
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    bl      __aeabi_ldivmod                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r2,r3}     @ vAA/vAA+1<- r2/r3
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 14-17 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_AND_LONG: /* 0xa0 */
/* File: armv5te/OP_AND_LONG.S */
/* File: armv5te/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[BB]
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[CC]
    ldmia   r2, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r3, {r2-r3}                 @ r2/r3<- vCC/vCC+1
    .if 0
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    and     r0, r0, r2                           @ optional op; may set condition codes
    and     r1, r1, r3                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 14-17 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_OR_LONG: /* 0xa1 */
/* File: armv5te/OP_OR_LONG.S */
/* File: armv5te/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[BB]
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[CC]
    ldmia   r2, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r3, {r2-r3}                 @ r2/r3<- vCC/vCC+1
    .if 0
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    orr     r0, r0, r2                           @ optional op; may set condition codes
    orr     r1, r1, r3                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 14-17 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_XOR_LONG: /* 0xa2 */
/* File: armv5te/OP_XOR_LONG.S */
/* File: armv5te/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[BB]
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[CC]
    ldmia   r2, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r3, {r2-r3}                 @ r2/r3<- vCC/vCC+1
    .if 0
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    eor     r0, r0, r2                           @ optional op; may set condition codes
    eor     r1, r1, r3                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 14-17 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_SHL_LONG: /* 0xa3 */
/* File: armv5te/OP_SHL_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shl-long vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r3, r0, #255                @ r3<- BB
    mov     r0, r0, lsr #8              @ r0<- CC
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[BB]
    GET_VREG(r2, r0)                    @ r2<- vCC
    ldmia   r3, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]

    mov     r1, r1, asl r2              @  r1<- r1 << r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r1, r1, r0, lsr r3          @  r1<- r1 | (r0 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r1, r0, asl ip              @  if r2 >= 32, r1<- r0 << (r2-32)
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    b       .LOP_SHL_LONG_finish

/* ------------------------------ */
    .balign 64
.L_OP_SHR_LONG: /* 0xa4 */
/* File: armv5te/OP_SHR_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shr-long vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r3, r0, #255                @ r3<- BB
    mov     r0, r0, lsr #8              @ r0<- CC
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[BB]
    GET_VREG(r2, r0)                    @ r2<- vCC
    ldmia   r3, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    and     r2, r2, #63                 @ r0<- r0 & 0x3f
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]

    mov     r0, r0, lsr r2              @  r0<- r2 >> r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @  r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r0, r1, asr ip              @  if r2 >= 32, r0<-r1 >> (r2-32)
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    b       .LOP_SHR_LONG_finish

/* ------------------------------ */
    .balign 64
.L_OP_USHR_LONG: /* 0xa5 */
/* File: armv5te/OP_USHR_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* ushr-long vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r3, r0, #255                @ r3<- BB
    mov     r0, r0, lsr #8              @ r0<- CC
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[BB]
    GET_VREG(r2, r0)                    @ r2<- vCC
    ldmia   r3, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    and     r2, r2, #63                 @ r0<- r0 & 0x3f
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]

    mov     r0, r0, lsr r2              @  r0<- r2 >> r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @  r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r0, r1, lsr ip              @  if r2 >= 32, r0<-r1 >>> (r2-32)
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    b       .LOP_USHR_LONG_finish

/* ------------------------------ */
    .balign 64
.L_OP_ADD_FLOAT: /* 0xa6 */
/* File: arm-vfp/OP_ADD_FLOAT.S */
/* File: arm-vfp/fbinop.S */
    /*
     * Generic 32-bit floating-point operation.  Provide an "instr" line that
     * specifies an instruction that performs "s2 = s0 op s1".  Because we
     * use the "softfp" ABI, this must be an instruction, not a function call.
     *
     * For: add-float, sub-float, mul-float, div-float
     */
    /* floatop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    flds    s1, [r3]                    @ s1<- vCC
    flds    s0, [r2]                    @ s0<- vBB

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    fadds   s2, s0, s1                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vAA
    fsts    s2, [r9]                    @ vAA<- s2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SUB_FLOAT: /* 0xa7 */
/* File: arm-vfp/OP_SUB_FLOAT.S */
/* File: arm-vfp/fbinop.S */
    /*
     * Generic 32-bit floating-point operation.  Provide an "instr" line that
     * specifies an instruction that performs "s2 = s0 op s1".  Because we
     * use the "softfp" ABI, this must be an instruction, not a function call.
     *
     * For: add-float, sub-float, mul-float, div-float
     */
    /* floatop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    flds    s1, [r3]                    @ s1<- vCC
    flds    s0, [r2]                    @ s0<- vBB

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    fsubs   s2, s0, s1                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vAA
    fsts    s2, [r9]                    @ vAA<- s2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_MUL_FLOAT: /* 0xa8 */
/* File: arm-vfp/OP_MUL_FLOAT.S */
/* File: arm-vfp/fbinop.S */
    /*
     * Generic 32-bit floating-point operation.  Provide an "instr" line that
     * specifies an instruction that performs "s2 = s0 op s1".  Because we
     * use the "softfp" ABI, this must be an instruction, not a function call.
     *
     * For: add-float, sub-float, mul-float, div-float
     */
    /* floatop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    flds    s1, [r3]                    @ s1<- vCC
    flds    s0, [r2]                    @ s0<- vBB

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    fmuls   s2, s0, s1                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vAA
    fsts    s2, [r9]                    @ vAA<- s2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_DIV_FLOAT: /* 0xa9 */
/* File: arm-vfp/OP_DIV_FLOAT.S */
/* File: arm-vfp/fbinop.S */
    /*
     * Generic 32-bit floating-point operation.  Provide an "instr" line that
     * specifies an instruction that performs "s2 = s0 op s1".  Because we
     * use the "softfp" ABI, this must be an instruction, not a function call.
     *
     * For: add-float, sub-float, mul-float, div-float
     */
    /* floatop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    flds    s1, [r3]                    @ s1<- vCC
    flds    s0, [r2]                    @ s0<- vBB

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    fdivs   s2, s0, s1                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vAA
    fsts    s2, [r9]                    @ vAA<- s2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_REM_FLOAT: /* 0xaa */
/* File: armv5te/OP_REM_FLOAT.S */
/* EABI doesn't define a float remainder function, but libm does */
/* File: armv5te/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
                               @ optional op; may set condition codes
    bl      fmodf                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_ADD_DOUBLE: /* 0xab */
/* File: arm-vfp/OP_ADD_DOUBLE.S */
/* File: arm-vfp/fbinopWide.S */
    /*
     * Generic 64-bit double-precision floating point binary operation.
     * Provide an "instr" line that specifies an instruction that performs
     * "d2 = d0 op d1".
     *
     * for: add-double, sub-double, mul-double, div-double
     */
    /* doubleop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    fldd    d1, [r3]                    @ d1<- vCC
    fldd    d0, [r2]                    @ d0<- vBB

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    faddd   d2, d0, d1                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vAA
    fstd    d2, [r9]                    @ vAA<- d2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SUB_DOUBLE: /* 0xac */
/* File: arm-vfp/OP_SUB_DOUBLE.S */
/* File: arm-vfp/fbinopWide.S */
    /*
     * Generic 64-bit double-precision floating point binary operation.
     * Provide an "instr" line that specifies an instruction that performs
     * "d2 = d0 op d1".
     *
     * for: add-double, sub-double, mul-double, div-double
     */
    /* doubleop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    fldd    d1, [r3]                    @ d1<- vCC
    fldd    d0, [r2]                    @ d0<- vBB

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    fsubd   d2, d0, d1                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vAA
    fstd    d2, [r9]                    @ vAA<- d2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_MUL_DOUBLE: /* 0xad */
/* File: arm-vfp/OP_MUL_DOUBLE.S */
/* File: arm-vfp/fbinopWide.S */
    /*
     * Generic 64-bit double-precision floating point binary operation.
     * Provide an "instr" line that specifies an instruction that performs
     * "d2 = d0 op d1".
     *
     * for: add-double, sub-double, mul-double, div-double
     */
    /* doubleop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    fldd    d1, [r3]                    @ d1<- vCC
    fldd    d0, [r2]                    @ d0<- vBB

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    fmuld   d2, d0, d1                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vAA
    fstd    d2, [r9]                    @ vAA<- d2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_DIV_DOUBLE: /* 0xae */
/* File: arm-vfp/OP_DIV_DOUBLE.S */
/* File: arm-vfp/fbinopWide.S */
    /*
     * Generic 64-bit double-precision floating point binary operation.
     * Provide an "instr" line that specifies an instruction that performs
     * "d2 = d0 op d1".
     *
     * for: add-double, sub-double, mul-double, div-double
     */
    /* doubleop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    fldd    d1, [r3]                    @ d1<- vCC
    fldd    d0, [r2]                    @ d0<- vBB

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    fdivd   d2, d0, d1                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vAA
    fstd    d2, [r9]                    @ vAA<- d2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_REM_DOUBLE: /* 0xaf */
/* File: armv5te/OP_REM_DOUBLE.S */
/* EABI doesn't define a double remainder function, but libm does */
/* File: armv5te/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[BB]
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[CC]
    ldmia   r2, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r3, {r2-r3}                 @ r2/r3<- vCC/vCC+1
    .if 0
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    bl      fmod                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 14-17 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_ADD_INT_2ADDR: /* 0xb0 */
/* File: armv5te/OP_ADD_INT_2ADDR.S */
/* File: armv5te/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r1, r3)                    @ r1<- vB
    GET_VREG(r0, r9)                    @ r0<- vA
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    add     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_SUB_INT_2ADDR: /* 0xb1 */
/* File: armv5te/OP_SUB_INT_2ADDR.S */
/* File: armv5te/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r1, r3)                    @ r1<- vB
    GET_VREG(r0, r9)                    @ r0<- vA
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    sub     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_MUL_INT_2ADDR: /* 0xb2 */
/* File: armv5te/OP_MUL_INT_2ADDR.S */
/* must be "mul r0, r1, r0" -- "r0, r0, r1" is illegal */
/* File: armv5te/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r1, r3)                    @ r1<- vB
    GET_VREG(r0, r9)                    @ r0<- vA
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    mul     r0, r1, r0                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_DIV_INT_2ADDR: /* 0xb3 */
/* File: armv5te/OP_DIV_INT_2ADDR.S */
/* File: armv5te/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r1, r3)                    @ r1<- vB
    GET_VREG(r0, r9)                    @ r0<- vA
    .if 1
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    bl     __aeabi_idiv                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_REM_INT_2ADDR: /* 0xb4 */
/* File: armv5te/OP_REM_INT_2ADDR.S */
/* idivmod returns quotient in r0 and remainder in r1 */
/* File: armv5te/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r1, r3)                    @ r1<- vB
    GET_VREG(r0, r9)                    @ r0<- vA
    .if 1
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    bl      __aeabi_idivmod                              @ r1<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r1, r9)               @ vAA<- r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_AND_INT_2ADDR: /* 0xb5 */
/* File: armv5te/OP_AND_INT_2ADDR.S */
/* File: armv5te/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r1, r3)                    @ r1<- vB
    GET_VREG(r0, r9)                    @ r0<- vA
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    and     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_OR_INT_2ADDR: /* 0xb6 */
/* File: armv5te/OP_OR_INT_2ADDR.S */
/* File: armv5te/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r1, r3)                    @ r1<- vB
    GET_VREG(r0, r9)                    @ r0<- vA
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    orr     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_XOR_INT_2ADDR: /* 0xb7 */
/* File: armv5te/OP_XOR_INT_2ADDR.S */
/* File: armv5te/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r1, r3)                    @ r1<- vB
    GET_VREG(r0, r9)                    @ r0<- vA
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    eor     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_SHL_INT_2ADDR: /* 0xb8 */
/* File: armv5te/OP_SHL_INT_2ADDR.S */
/* File: armv5te/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r1, r3)                    @ r1<- vB
    GET_VREG(r0, r9)                    @ r0<- vA
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

    and     r1, r1, #31                           @ optional op; may set condition codes
    mov     r0, r0, asl r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_SHR_INT_2ADDR: /* 0xb9 */
/* File: armv5te/OP_SHR_INT_2ADDR.S */
/* File: armv5te/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r1, r3)                    @ r1<- vB
    GET_VREG(r0, r9)                    @ r0<- vA
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

    and     r1, r1, #31                           @ optional op; may set condition codes
    mov     r0, r0, asr r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_USHR_INT_2ADDR: /* 0xba */
/* File: armv5te/OP_USHR_INT_2ADDR.S */
/* File: armv5te/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r1, r3)                    @ r1<- vB
    GET_VREG(r0, r9)                    @ r0<- vA
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

    and     r1, r1, #31                           @ optional op; may set condition codes
    mov     r0, r0, lsr r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_ADD_LONG_2ADDR: /* 0xbb */
/* File: armv5te/OP_ADD_LONG_2ADDR.S */
/* File: armv5te/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr, add-double/2addr,
     *      sub-double/2addr, mul-double/2addr, div-double/2addr,
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r9, r9, #15
    add     r1, rFP, r1, lsl #2         @ r1<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r1, {r2-r3}                 @ r2/r3<- vBB/vBB+1
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1
    .if 0
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

    adds    r0, r0, r2                           @ optional op; may set condition codes
    adc     r1, r1, r3                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-15 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_SUB_LONG_2ADDR: /* 0xbc */
/* File: armv5te/OP_SUB_LONG_2ADDR.S */
/* File: armv5te/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr, add-double/2addr,
     *      sub-double/2addr, mul-double/2addr, div-double/2addr,
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r9, r9, #15
    add     r1, rFP, r1, lsl #2         @ r1<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r1, {r2-r3}                 @ r2/r3<- vBB/vBB+1
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1
    .if 0
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

    subs    r0, r0, r2                           @ optional op; may set condition codes
    sbc     r1, r1, r3                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-15 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_MUL_LONG_2ADDR: /* 0xbd */
/* File: armv5te/OP_MUL_LONG_2ADDR.S */
    /*
     * Signed 64-bit integer multiply, "/2addr" version.
     *
     * See OP_MUL_LONG for an explanation.
     *
     * We get a little tight on registers, so to avoid looking up &fp[A]
     * again we stuff it into rINST.
     */
    /* mul-long/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r9, r9, #15
    add     r1, rFP, r1, lsl #2         @ r1<- &fp[B]
    add     rINST, rFP, r9, lsl #2      @ rINST<- &fp[A]
    ldmia   r1, {r2-r3}                 @ r2/r3<- vBB/vBB+1
    ldmia   rINST, {r0-r1}              @ r0/r1<- vAA/vAA+1
    mul     ip, r2, r1                  @  ip<- ZxW
    umull   r9, r10, r2, r0             @  r9/r10 <- ZxX
    mla     r2, r0, r3, ip              @  r2<- YxX + (ZxW)
    mov     r0, rINST                   @ r0<- &fp[A] (free up rINST)
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    add     r10, r2, r10                @  r10<- r10 + low(ZxW + (YxX))
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r0, {r9-r10}                @ vAA/vAA+1<- r9/r10
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_DIV_LONG_2ADDR: /* 0xbe */
/* File: armv5te/OP_DIV_LONG_2ADDR.S */
/* File: armv5te/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr, add-double/2addr,
     *      sub-double/2addr, mul-double/2addr, div-double/2addr,
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r9, r9, #15
    add     r1, rFP, r1, lsl #2         @ r1<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r1, {r2-r3}                 @ r2/r3<- vBB/vBB+1
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1
    .if 1
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    bl      __aeabi_ldivmod                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-15 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_REM_LONG_2ADDR: /* 0xbf */
/* File: armv5te/OP_REM_LONG_2ADDR.S */
/* ldivmod returns quotient in r0/r1 and remainder in r2/r3 */
/* File: armv5te/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr, add-double/2addr,
     *      sub-double/2addr, mul-double/2addr, div-double/2addr,
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r9, r9, #15
    add     r1, rFP, r1, lsl #2         @ r1<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r1, {r2-r3}                 @ r2/r3<- vBB/vBB+1
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1
    .if 1
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    bl      __aeabi_ldivmod                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r2,r3}     @ vAA/vAA+1<- r2/r3
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-15 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_AND_LONG_2ADDR: /* 0xc0 */
/* File: armv5te/OP_AND_LONG_2ADDR.S */
/* File: armv5te/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr, add-double/2addr,
     *      sub-double/2addr, mul-double/2addr, div-double/2addr,
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r9, r9, #15
    add     r1, rFP, r1, lsl #2         @ r1<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r1, {r2-r3}                 @ r2/r3<- vBB/vBB+1
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1
    .if 0
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

    and     r0, r0, r2                           @ optional op; may set condition codes
    and     r1, r1, r3                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-15 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_OR_LONG_2ADDR: /* 0xc1 */
/* File: armv5te/OP_OR_LONG_2ADDR.S */
/* File: armv5te/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr, add-double/2addr,
     *      sub-double/2addr, mul-double/2addr, div-double/2addr,
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r9, r9, #15
    add     r1, rFP, r1, lsl #2         @ r1<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r1, {r2-r3}                 @ r2/r3<- vBB/vBB+1
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1
    .if 0
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

    orr     r0, r0, r2                           @ optional op; may set condition codes
    orr     r1, r1, r3                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-15 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_XOR_LONG_2ADDR: /* 0xc2 */
/* File: armv5te/OP_XOR_LONG_2ADDR.S */
/* File: armv5te/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr, add-double/2addr,
     *      sub-double/2addr, mul-double/2addr, div-double/2addr,
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r9, r9, #15
    add     r1, rFP, r1, lsl #2         @ r1<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r1, {r2-r3}                 @ r2/r3<- vBB/vBB+1
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1
    .if 0
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

    eor     r0, r0, r2                           @ optional op; may set condition codes
    eor     r1, r1, r3                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-15 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_SHL_LONG_2ADDR: /* 0xc3 */
/* File: armv5te/OP_SHL_LONG_2ADDR.S */
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shl-long/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r2, r3)                    @ r2<- vB
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1

    mov     r1, r1, asl r2              @  r1<- r1 << r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r1, r1, r0, lsr r3          @  r1<- r1 | (r0 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    movpl   r1, r0, asl ip              @  if r2 >= 32, r1<- r0 << (r2-32)
    mov     r0, r0, asl r2              @  r0<- r0 << r2
    b       .LOP_SHL_LONG_2ADDR_finish

/* ------------------------------ */
    .balign 64
.L_OP_SHR_LONG_2ADDR: /* 0xc4 */
/* File: armv5te/OP_SHR_LONG_2ADDR.S */
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shr-long/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r2, r3)                    @ r2<- vB
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1

    mov     r0, r0, lsr r2              @  r0<- r2 >> r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @  r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    movpl   r0, r1, asr ip              @  if r2 >= 32, r0<-r1 >> (r2-32)
    mov     r1, r1, asr r2              @  r1<- r1 >> r2
    b       .LOP_SHR_LONG_2ADDR_finish

/* ------------------------------ */
    .balign 64
.L_OP_USHR_LONG_2ADDR: /* 0xc5 */
/* File: armv5te/OP_USHR_LONG_2ADDR.S */
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* ushr-long/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r2, r3)                    @ r2<- vB
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1

    mov     r0, r0, lsr r2              @  r0<- r2 >> r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @  r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    movpl   r0, r1, lsr ip              @  if r2 >= 32, r0<-r1 >>> (r2-32)
    mov     r1, r1, lsr r2              @  r1<- r1 >>> r2
    b       .LOP_USHR_LONG_2ADDR_finish

/* ------------------------------ */
    .balign 64
.L_OP_ADD_FLOAT_2ADDR: /* 0xc6 */
/* File: arm-vfp/OP_ADD_FLOAT_2ADDR.S */
/* File: arm-vfp/fbinop2addr.S */
    /*
     * Generic 32-bit floating point "/2addr" binary operation.  Provide
     * an "instr" line that specifies an instruction that performs
     * "s2 = s0 op s1".
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr, div-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    and     r9, r9, #15                 @ r9<- A
    flds    s1, [r3]                    @ s1<- vB
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    flds    s0, [r9]                    @ s0<- vA

    fadds   s2, s0, s1                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    fsts    s2, [r9]                    @ vAA<- s2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SUB_FLOAT_2ADDR: /* 0xc7 */
/* File: arm-vfp/OP_SUB_FLOAT_2ADDR.S */
/* File: arm-vfp/fbinop2addr.S */
    /*
     * Generic 32-bit floating point "/2addr" binary operation.  Provide
     * an "instr" line that specifies an instruction that performs
     * "s2 = s0 op s1".
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr, div-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    and     r9, r9, #15                 @ r9<- A
    flds    s1, [r3]                    @ s1<- vB
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    flds    s0, [r9]                    @ s0<- vA

    fsubs   s2, s0, s1                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    fsts    s2, [r9]                    @ vAA<- s2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_MUL_FLOAT_2ADDR: /* 0xc8 */
/* File: arm-vfp/OP_MUL_FLOAT_2ADDR.S */
/* File: arm-vfp/fbinop2addr.S */
    /*
     * Generic 32-bit floating point "/2addr" binary operation.  Provide
     * an "instr" line that specifies an instruction that performs
     * "s2 = s0 op s1".
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr, div-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    and     r9, r9, #15                 @ r9<- A
    flds    s1, [r3]                    @ s1<- vB
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    flds    s0, [r9]                    @ s0<- vA

    fmuls   s2, s0, s1                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    fsts    s2, [r9]                    @ vAA<- s2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_DIV_FLOAT_2ADDR: /* 0xc9 */
/* File: arm-vfp/OP_DIV_FLOAT_2ADDR.S */
/* File: arm-vfp/fbinop2addr.S */
    /*
     * Generic 32-bit floating point "/2addr" binary operation.  Provide
     * an "instr" line that specifies an instruction that performs
     * "s2 = s0 op s1".
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr, div-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    and     r9, r9, #15                 @ r9<- A
    flds    s1, [r3]                    @ s1<- vB
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    flds    s0, [r9]                    @ s0<- vA

    fdivs   s2, s0, s1                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    fsts    s2, [r9]                    @ vAA<- s2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_REM_FLOAT_2ADDR: /* 0xca */
/* File: armv5te/OP_REM_FLOAT_2ADDR.S */
/* EABI doesn't define a float remainder function, but libm does */
/* File: armv5te/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r1, r3)                    @ r1<- vB
    GET_VREG(r0, r9)                    @ r0<- vA
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    bl      fmodf                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_ADD_DOUBLE_2ADDR: /* 0xcb */
/* File: arm-vfp/OP_ADD_DOUBLE_2ADDR.S */
/* File: arm-vfp/fbinopWide2addr.S */
    /*
     * Generic 64-bit floating point "/2addr" binary operation.  Provide
     * an "instr" line that specifies an instruction that performs
     * "d2 = d0 op d1".
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *      div-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    and     r9, r9, #15                 @ r9<- A
    fldd    d1, [r3]                    @ d1<- vB
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    fldd    d0, [r9]                    @ d0<- vA

    faddd   d2, d0, d1                              @ d2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    fstd    d2, [r9]                    @ vAA<- d2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SUB_DOUBLE_2ADDR: /* 0xcc */
/* File: arm-vfp/OP_SUB_DOUBLE_2ADDR.S */
/* File: arm-vfp/fbinopWide2addr.S */
    /*
     * Generic 64-bit floating point "/2addr" binary operation.  Provide
     * an "instr" line that specifies an instruction that performs
     * "d2 = d0 op d1".
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *      div-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    and     r9, r9, #15                 @ r9<- A
    fldd    d1, [r3]                    @ d1<- vB
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    fldd    d0, [r9]                    @ d0<- vA

    fsubd   d2, d0, d1                              @ d2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    fstd    d2, [r9]                    @ vAA<- d2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_MUL_DOUBLE_2ADDR: /* 0xcd */
/* File: arm-vfp/OP_MUL_DOUBLE_2ADDR.S */
/* File: arm-vfp/fbinopWide2addr.S */
    /*
     * Generic 64-bit floating point "/2addr" binary operation.  Provide
     * an "instr" line that specifies an instruction that performs
     * "d2 = d0 op d1".
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *      div-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    and     r9, r9, #15                 @ r9<- A
    fldd    d1, [r3]                    @ d1<- vB
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    fldd    d0, [r9]                    @ d0<- vA

    fmuld   d2, d0, d1                              @ d2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    fstd    d2, [r9]                    @ vAA<- d2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_DIV_DOUBLE_2ADDR: /* 0xce */
/* File: arm-vfp/OP_DIV_DOUBLE_2ADDR.S */
/* File: arm-vfp/fbinopWide2addr.S */
    /*
     * Generic 64-bit floating point "/2addr" binary operation.  Provide
     * an "instr" line that specifies an instruction that performs
     * "d2 = d0 op d1".
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *      div-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    and     r9, r9, #15                 @ r9<- A
    fldd    d1, [r3]                    @ d1<- vB
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    fldd    d0, [r9]                    @ d0<- vA

    fdivd   d2, d0, d1                              @ d2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    fstd    d2, [r9]                    @ vAA<- d2
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_REM_DOUBLE_2ADDR: /* 0xcf */
/* File: armv5te/OP_REM_DOUBLE_2ADDR.S */
/* EABI doesn't define a double remainder function, but libm does */
/* File: armv5te/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr, add-double/2addr,
     *      sub-double/2addr, mul-double/2addr, div-double/2addr,
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r9, r9, #15
    add     r1, rFP, r1, lsl #2         @ r1<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r1, {r2-r3}                 @ r2/r3<- vBB/vBB+1
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1
    .if 0
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    bl      fmod                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0,r1}     @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-15 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_ADD_INT_LIT16: /* 0xd0 */
/* File: armv5te/OP_ADD_INT_LIT16.S */
/* File: armv5te/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    FETCH_S(r1, 1)                      @ r1<- ssssCCCC (sign-extended)
    mov     r2, rINST, lsr #12          @ r2<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r2)                    @ r0<- vB
    and     r9, r9, #15
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    add     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_RSUB_INT: /* 0xd1 */
/* File: armv5te/OP_RSUB_INT.S */
/* this op is "rsub-int", but can be thought of as "rsub-int/lit16" */
/* File: armv5te/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    FETCH_S(r1, 1)                      @ r1<- ssssCCCC (sign-extended)
    mov     r2, rINST, lsr #12          @ r2<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r2)                    @ r0<- vB
    and     r9, r9, #15
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    rsb     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_MUL_INT_LIT16: /* 0xd2 */
/* File: armv5te/OP_MUL_INT_LIT16.S */
/* must be "mul r0, r1, r0" -- "r0, r0, r1" is illegal */
/* File: armv5te/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    FETCH_S(r1, 1)                      @ r1<- ssssCCCC (sign-extended)
    mov     r2, rINST, lsr #12          @ r2<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r2)                    @ r0<- vB
    and     r9, r9, #15
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    mul     r0, r1, r0                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_DIV_INT_LIT16: /* 0xd3 */
/* File: armv5te/OP_DIV_INT_LIT16.S */
/* File: armv5te/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    FETCH_S(r1, 1)                      @ r1<- ssssCCCC (sign-extended)
    mov     r2, rINST, lsr #12          @ r2<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r2)                    @ r0<- vB
    and     r9, r9, #15
    .if 1
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    bl     __aeabi_idiv                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_REM_INT_LIT16: /* 0xd4 */
/* File: armv5te/OP_REM_INT_LIT16.S */
/* idivmod returns quotient in r0 and remainder in r1 */
/* File: armv5te/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    FETCH_S(r1, 1)                      @ r1<- ssssCCCC (sign-extended)
    mov     r2, rINST, lsr #12          @ r2<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r2)                    @ r0<- vB
    and     r9, r9, #15
    .if 1
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    bl      __aeabi_idivmod                              @ r1<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r1, r9)               @ vAA<- r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_AND_INT_LIT16: /* 0xd5 */
/* File: armv5te/OP_AND_INT_LIT16.S */
/* File: armv5te/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    FETCH_S(r1, 1)                      @ r1<- ssssCCCC (sign-extended)
    mov     r2, rINST, lsr #12          @ r2<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r2)                    @ r0<- vB
    and     r9, r9, #15
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    and     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_OR_INT_LIT16: /* 0xd6 */
/* File: armv5te/OP_OR_INT_LIT16.S */
/* File: armv5te/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    FETCH_S(r1, 1)                      @ r1<- ssssCCCC (sign-extended)
    mov     r2, rINST, lsr #12          @ r2<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r2)                    @ r0<- vB
    and     r9, r9, #15
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    orr     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_XOR_INT_LIT16: /* 0xd7 */
/* File: armv5te/OP_XOR_INT_LIT16.S */
/* File: armv5te/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    FETCH_S(r1, 1)                      @ r1<- ssssCCCC (sign-extended)
    mov     r2, rINST, lsr #12          @ r2<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r2)                    @ r0<- vB
    and     r9, r9, #15
    .if 0
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    eor     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_ADD_INT_LIT8: /* 0xd8 */
/* File: armv5te/OP_ADD_INT_LIT8.S */
/* File: armv5te/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S(r3, 1)                      @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG(r0, r2)                    @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    .if 0
    @cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    add     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-12 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_RSUB_INT_LIT8: /* 0xd9 */
/* File: armv5te/OP_RSUB_INT_LIT8.S */
/* File: armv5te/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S(r3, 1)                      @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG(r0, r2)                    @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    .if 0
    @cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    rsb     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-12 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_MUL_INT_LIT8: /* 0xda */
/* File: armv5te/OP_MUL_INT_LIT8.S */
/* must be "mul r0, r1, r0" -- "r0, r0, r1" is illegal */
/* File: armv5te/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S(r3, 1)                      @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG(r0, r2)                    @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    .if 0
    @cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    mul     r0, r1, r0                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-12 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_DIV_INT_LIT8: /* 0xdb */
/* File: armv5te/OP_DIV_INT_LIT8.S */
/* File: armv5te/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S(r3, 1)                      @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG(r0, r2)                    @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    .if 1
    @cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    bl     __aeabi_idiv                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-12 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_REM_INT_LIT8: /* 0xdc */
/* File: armv5te/OP_REM_INT_LIT8.S */
/* idivmod returns quotient in r0 and remainder in r1 */
/* File: armv5te/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S(r3, 1)                      @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG(r0, r2)                    @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    .if 1
    @cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    bl      __aeabi_idivmod                              @ r1<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r1, r9)               @ vAA<- r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-12 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_AND_INT_LIT8: /* 0xdd */
/* File: armv5te/OP_AND_INT_LIT8.S */
/* File: armv5te/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S(r3, 1)                      @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG(r0, r2)                    @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    .if 0
    @cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    and     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-12 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_OR_INT_LIT8: /* 0xde */
/* File: armv5te/OP_OR_INT_LIT8.S */
/* File: armv5te/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S(r3, 1)                      @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG(r0, r2)                    @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    .if 0
    @cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    orr     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-12 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_XOR_INT_LIT8: /* 0xdf */
/* File: armv5te/OP_XOR_INT_LIT8.S */
/* File: armv5te/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S(r3, 1)                      @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG(r0, r2)                    @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    .if 0
    @cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

                               @ optional op; may set condition codes
    eor     r0, r0, r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-12 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_SHL_INT_LIT8: /* 0xe0 */
/* File: armv5te/OP_SHL_INT_LIT8.S */
/* File: armv5te/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S(r3, 1)                      @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG(r0, r2)                    @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    .if 0
    @cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    and     r1, r1, #31                           @ optional op; may set condition codes
    mov     r0, r0, asl r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-12 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_SHR_INT_LIT8: /* 0xe1 */
/* File: armv5te/OP_SHR_INT_LIT8.S */
/* File: armv5te/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S(r3, 1)                      @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG(r0, r2)                    @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    .if 0
    @cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    and     r1, r1, #31                           @ optional op; may set condition codes
    mov     r0, r0, asr r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-12 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_USHR_INT_LIT8: /* 0xe2 */
/* File: armv5te/OP_USHR_INT_LIT8.S */
/* File: armv5te/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S(r3, 1)                      @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG(r0, r2)                    @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    .if 0
    @cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    and     r1, r1, #31                           @ optional op; may set condition codes
    mov     r0, r0, lsr r1                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)               @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-12 instructions */


/* ------------------------------ */
    .balign 64
.L_OP_IGET_VOLATILE: /* 0xe3 */
/* File: armv5te/OP_IGET_VOLATILE.S */
/* File: armv5te/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IGET_VOLATILE_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0
    bne     .LOP_IGET_VOLATILE_finish
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_IPUT_VOLATILE: /* 0xe4 */
/* File: armv5te/OP_IPUT_VOLATILE.S */
/* File: armv5te/OP_IPUT.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IPUT_VOLATILE_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ success?
    bne     .LOP_IPUT_VOLATILE_finish          @ yes, finish up
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_SGET_VOLATILE: /* 0xe5 */
/* File: armv5te/OP_SGET_VOLATILE.S */
/* File: armv5te/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SGET_VOLATILE_resolve         @ yes, do resolve
.LOP_SGET_VOLATILE_finish: @ field ptr in r0
    ldr     r1, [r0, #offStaticField_value] @ r1<- field value
    SMP_DMB                            @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r2)                    @ fp[AA]<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SPUT_VOLATILE: /* 0xe6 */
/* File: armv5te/OP_SPUT_VOLATILE.S */
/* File: armv5te/OP_SPUT.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]        @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SPUT_VOLATILE_resolve         @ yes, do resolve
.LOP_SPUT_VOLATILE_finish:   @ field ptr in r0
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r1, r2)                    @ r1<- fp[AA]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SMP_DMB_ST                        @ releasing store
    str     r1, [r0, #offStaticField_value] @ field<- vAA
    SMP_DMB
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IGET_OBJECT_VOLATILE: /* 0xe7 */
/* File: armv5te/OP_IGET_OBJECT_VOLATILE.S */
/* File: armv5te/OP_IGET.S */
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IGET_OBJECT_VOLATILE_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0
    bne     .LOP_IGET_OBJECT_VOLATILE_finish
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_IGET_WIDE_VOLATILE: /* 0xe8 */
/* File: armv5te/OP_IGET_WIDE_VOLATILE.S */
/* File: armv5te/OP_IGET_WIDE.S */
    /*
     * Wide 32-bit instance field get.
     */
    /* iget-wide vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IGET_WIDE_VOLATILE_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method] @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0
    bne     .LOP_IGET_WIDE_VOLATILE_finish
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_IPUT_WIDE_VOLATILE: /* 0xe9 */
/* File: armv5te/OP_IPUT_WIDE_VOLATILE.S */
/* File: armv5te/OP_IPUT_WIDE.S */
    /* iput-wide vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IPUT_WIDE_VOLATILE_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method] @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ success?
    bne     .LOP_IPUT_WIDE_VOLATILE_finish          @ yes, finish up
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_SGET_WIDE_VOLATILE: /* 0xea */
/* File: armv5te/OP_SGET_WIDE_VOLATILE.S */
/* File: armv5te/OP_SGET_WIDE.S */
    /*
     * 64-bit SGET handler.
     */
    /* sget-wide vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SGET_WIDE_VOLATILE_resolve         @ yes, do resolve
.LOP_SGET_WIDE_VOLATILE_finish:
    mov     r9, rINST, lsr #8           @ r9<- AA
    .if 1
    add     r0, r0, #offStaticField_value @ r0<- pointer to data
    bl      dvmQuasiAtomicRead64        @ r0/r1<- contents of field
    .else
    ldrd    r0, [r0, #offStaticField_value] @ r0/r1<- field value (aligned)
    .endif
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    stmia   r9, {r0-r1}                 @ vAA/vAA+1<- r0/r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SPUT_WIDE_VOLATILE: /* 0xeb */
/* File: armv5te/OP_SPUT_WIDE_VOLATILE.S */
/* File: armv5te/OP_SPUT_WIDE.S */
    /*
     * 64-bit SPUT handler.
     */
    /* sput-wide vAA, field@BBBB */
    ldr     r0, [rSELF, #offThread_methodClassDex]  @ r0<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r0, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    mov     r9, rINST, lsr #8           @ r9<- AA
    ldr     r2, [r10, r1, lsl #2]        @ r2<- resolved StaticField ptr
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    cmp     r2, #0                      @ is resolved entry null?
    beq     .LOP_SPUT_WIDE_VOLATILE_resolve         @ yes, do resolve
.LOP_SPUT_WIDE_VOLATILE_finish: @ field ptr in r2, AA in r9
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1
    GET_INST_OPCODE(r10)                @ extract opcode from rINST
    .if 1
    add     r2, r2, #offStaticField_value @ r2<- pointer to data
    bl      dvmQuasiAtomicSwap64Sync    @ stores r0/r1 into addr r2
    .else
    strd    r0, [r2, #offStaticField_value] @ field<- vAA/vAA+1
    .endif
    GOTO_OPCODE(r10)                    @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_BREAKPOINT: /* 0xec */
/* File: armv5te/OP_BREAKPOINT.S */
    /*
     * Breakpoint handler.
     *
     * Restart this instruction with the original opcode.  By
     * the time we get here, the breakpoint will have already been
     * handled.
     */
    mov     r0, rPC
    bl      dvmGetOriginalOpcode        @ (rPC)
    FETCH(rINST, 0)                     @ reload OP_BREAKPOINT + rest of inst
    ldr     r1, [rSELF, #offThread_mainHandlerTable]
    and     rINST, #0xff00
    orr     rINST, rINST, r0
    GOTO_OPCODE_BASE(r1, r0)

/* ------------------------------ */
    .balign 64
.L_OP_THROW_VERIFICATION_ERROR: /* 0xed */
/* File: armv5te/OP_THROW_VERIFICATION_ERROR.S */
    /*
     * Handle a throw-verification-error instruction.  This throws an
     * exception for an error discovered during verification.  The
     * exception is indicated by AA, with some detail provided by BBBB.
     */
    /* op AA, ref@BBBB */
    ldr     r0, [rSELF, #offThread_method]    @ r0<- self->method
    FETCH(r2, 1)                        @ r2<- BBBB
    EXPORT_PC()                         @ export the PC
    mov     r1, rINST, lsr #8           @ r1<- AA
    bl      dvmThrowVerificationError   @ always throws
    b       common_exceptionThrown      @ handle exception

/* ------------------------------ */
    .balign 64
.L_OP_EXECUTE_INLINE: /* 0xee */
/* File: armv5te/OP_EXECUTE_INLINE.S */
    /*
     * Execute a "native inline" instruction.
     *
     * We need to call an InlineOp4Func:
     *  bool (func)(u4 arg0, u4 arg1, u4 arg2, u4 arg3, JValue* pResult)
     *
     * The first four args are in r0-r3, pointer to return value storage
     * is on the stack.  The function's return value is a flag that tells
     * us if an exception was thrown.
     *
     * TUNING: could maintain two tables, pointer in Thread and
     * swap if profiler/debuggger active.
     */
    /* [opt] execute-inline vAA, {vC, vD, vE, vF}, inline@BBBB */
    ldrh    r2, [rSELF, #offThread_subMode]
    FETCH(r10, 1)                       @ r10<- BBBB
    EXPORT_PC()                         @ can throw
    ands    r2, #kSubModeDebugProfile   @ Any going on?
    bne     .LOP_EXECUTE_INLINE_debugmode       @ yes - take slow path
.LOP_EXECUTE_INLINE_resume:
    add     r1, rSELF, #offThread_retval  @ r1<- &self->retval
    sub     sp, sp, #8                  @ make room for arg, +64 bit align
    mov     r0, rINST, lsr #12          @ r0<- B
    str     r1, [sp]                    @ push &self->retval
    bl      .LOP_EXECUTE_INLINE_continue        @ make call; will return after
    add     sp, sp, #8                  @ pop stack
    cmp     r0, #0                      @ test boolean result of inline
    beq     common_exceptionThrown      @ returned false, handle exception
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_EXECUTE_INLINE_RANGE: /* 0xef */
/* File: armv5te/OP_EXECUTE_INLINE_RANGE.S */
    /*
     * Execute a "native inline" instruction, using "/range" semantics.
     * Same idea as execute-inline, but we get the args differently.
     *
     * We need to call an InlineOp4Func:
     *  bool (func)(u4 arg0, u4 arg1, u4 arg2, u4 arg3, JValue* pResult)
     *
     * The first four args are in r0-r3, pointer to return value storage
     * is on the stack.  The function's return value is a flag that tells
     * us if an exception was thrown.
     */
    /* [opt] execute-inline/range {vCCCC..v(CCCC+AA-1)}, inline@BBBB */
    ldrh    r2, [rSELF, #offThread_subMode]
    FETCH(r10, 1)                       @ r10<- BBBB
    EXPORT_PC()                         @ can throw
    ands    r2, #kSubModeDebugProfile   @ Any going on?
    bne     .LOP_EXECUTE_INLINE_RANGE_debugmode       @ yes - take slow path
.LOP_EXECUTE_INLINE_RANGE_resume:
    add     r1, rSELF, #offThread_retval  @ r1<- &self->retval
    sub     sp, sp, #8                  @ make room for arg, +64 bit align
    mov     r0, rINST, lsr #8           @ r0<- AA
    str     r1, [sp]                    @ push &self->retval
    bl      .LOP_EXECUTE_INLINE_RANGE_continue        @ make call; will return after
    add     sp, sp, #8                  @ pop stack
    cmp     r0, #0                      @ test boolean result of inline
    beq     common_exceptionThrown      @ returned false, handle exception
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_OBJECT_INIT_RANGE: /* 0xf0 */
/* File: armv5te/OP_INVOKE_OBJECT_INIT_RANGE.S */
    /*
     * Invoke Object.<init> on an object.  In practice we know that
     * Object's nullary constructor doesn't do anything, so we just
     * skip it unless a debugger is active.
     */
    FETCH(r1, 2)                  @ r1<- CCCC
    GET_VREG(r0, r1)                    @ r0<- "this" ptr
    cmp     r0, #0                      @ check for NULL
    beq     common_errNullObject        @ export PC and throw NPE
    ldr     r1, [r0, #offObject_clazz]  @ r1<- obj->clazz
    ldr     r2, [r1, #offClassObject_accessFlags] @ r2<- clazz->accessFlags
    tst     r2, #CLASS_ISFINALIZABLE    @ is this class finalizable?
    bne     .LOP_INVOKE_OBJECT_INIT_RANGE_setFinal        @ yes, go
.LOP_INVOKE_OBJECT_INIT_RANGE_finish:
    ldrh    r1, [rSELF, #offThread_subMode]
    ands    r1, #kSubModeDebuggerActive @ debugger active?
    bne     .LOP_INVOKE_OBJECT_INIT_RANGE_debugger        @ Yes - skip optimization
    FETCH_ADVANCE_INST(2+1)       @ advance to next instr, load rINST
    GET_INST_OPCODE(ip)                 @ ip<- opcode from rINST
    GOTO_OPCODE(ip)                     @ execute it

/* ------------------------------ */
    .balign 64
.L_OP_RETURN_VOID_BARRIER: /* 0xf1 */
/* File: armv5te/OP_RETURN_VOID_BARRIER.S */
    SMP_DMB_ST
    b       common_returnFromMethod

/* ------------------------------ */
    .balign 64
.L_OP_IGET_QUICK: /* 0xf2 */
/* File: armv5te/OP_IGET_QUICK.S */
    /* For: iget-quick, iget-object-quick */
    /* op vA, vB, offset@CCCC */
    mov     r2, rINST, lsr #12          @ r2<- B
    GET_VREG(r3, r2)                    @ r3<- object we're operating on
    FETCH(r1, 1)                        @ r1<- field byte offset
    cmp     r3, #0                      @ check object for null
    mov     r2, rINST, lsr #8           @ r2<- A(+)
    beq     common_errNullObject        @ object was null
    ldr     r0, [r3, r1]                @ r0<- obj.field (always 32 bits)
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[A]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_IGET_WIDE_QUICK: /* 0xf3 */
/* File: armv5te/OP_IGET_WIDE_QUICK.S */
    /* iget-wide-quick vA, vB, offset@CCCC */
    mov     r2, rINST, lsr #12          @ r2<- B
    GET_VREG(r3, r2)                    @ r3<- object we're operating on
    FETCH(ip, 1)                        @ ip<- field byte offset
    cmp     r3, #0                      @ check object for null
    mov     r2, rINST, lsr #8           @ r2<- A(+)
    beq     common_errNullObject        @ object was null
    ldrd    r0, [r3, ip]                @ r0<- obj.field (64 bits, aligned)
    and     r2, r2, #15
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    add     r3, rFP, r2, lsl #2         @ r3<- &fp[A]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r3, {r0-r1}                 @ fp[A]<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_IGET_OBJECT_QUICK: /* 0xf4 */
/* File: armv5te/OP_IGET_OBJECT_QUICK.S */
/* File: armv5te/OP_IGET_QUICK.S */
    /* For: iget-quick, iget-object-quick */
    /* op vA, vB, offset@CCCC */
    mov     r2, rINST, lsr #12          @ r2<- B
    GET_VREG(r3, r2)                    @ r3<- object we're operating on
    FETCH(r1, 1)                        @ r1<- field byte offset
    cmp     r3, #0                      @ check object for null
    mov     r2, rINST, lsr #8           @ r2<- A(+)
    beq     common_errNullObject        @ object was null
    ldr     r0, [r3, r1]                @ r0<- obj.field (always 32 bits)
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[A]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_IPUT_QUICK: /* 0xf5 */
/* File: armv5te/OP_IPUT_QUICK.S */
    /* For: iput-quick */
    /* op vA, vB, offset@CCCC */
    mov     r2, rINST, lsr #12          @ r2<- B
    GET_VREG(r3, r2)                    @ r3<- fp[B], the object pointer
    FETCH(r1, 1)                        @ r1<- field byte offset
    cmp     r3, #0                      @ check object for null
    mov     r2, rINST, lsr #8           @ r2<- A(+)
    beq     common_errNullObject        @ object was null
    and     r2, r2, #15
    GET_VREG(r0, r2)                    @ r0<- fp[A]
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    str     r0, [r3, r1]                @ obj.field (always 32 bits)<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_IPUT_WIDE_QUICK: /* 0xf6 */
/* File: armv5te/OP_IPUT_WIDE_QUICK.S */
    /* iput-wide-quick vA, vB, offset@CCCC */
    mov     r0, rINST, lsr #8           @ r0<- A(+)
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r0, r0, #15
    GET_VREG(r2, r1)                    @ r2<- fp[B], the object pointer
    add     r3, rFP, r0, lsl #2         @ r3<- &fp[A]
    cmp     r2, #0                      @ check object for null
    ldmia   r3, {r0-r1}                 @ r0/r1<- fp[A]
    beq     common_errNullObject        @ object was null
    FETCH(r3, 1)                        @ r3<- field byte offset
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    strd    r0, [r2, r3]                @ obj.field (64 bits, aligned)<- r0/r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_IPUT_OBJECT_QUICK: /* 0xf7 */
/* File: armv5te/OP_IPUT_OBJECT_QUICK.S */
    /* For: iput-object-quick */
    /* op vA, vB, offset@CCCC */
    mov     r2, rINST, lsr #12          @ r2<- B
    GET_VREG(r3, r2)                    @ r3<- fp[B], the object pointer
    FETCH(r1, 1)                        @ r1<- field byte offset
    cmp     r3, #0                      @ check object for null
    mov     r2, rINST, lsr #8           @ r2<- A(+)
    beq     common_errNullObject        @ object was null
    and     r2, r2, #15
    GET_VREG(r0, r2)                    @ r0<- fp[A]
    ldr     r2, [rSELF, #offThread_cardTable]  @ r2<- card table base
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    str     r0, [r3, r1]                @ obj.field (always 32 bits)<- r0
    cmp     r0, #0
    strneb  r2, [r2, r3, lsr #GC_CARD_SHIFT] @ mark card based on obj head
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_VIRTUAL_QUICK: /* 0xf8 */
/* File: armv5te/OP_INVOKE_VIRTUAL_QUICK.S */
    /*
     * Handle an optimized virtual method call.
     *
     * for: [opt] invoke-virtual-quick, invoke-virtual-quick/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(r3, 2)                        @ r3<- FEDC or CCCC
    FETCH(r1, 1)                        @ r1<- BBBB
    .if     (!0)
    and     r3, r3, #15                 @ r3<- C (or stays CCCC)
    .endif
    GET_VREG(r9, r3)                    @ r9<- vC ("this" ptr)
    cmp     r9, #0                      @ is "this" null?
    beq     common_errNullObject        @ null "this", throw exception
    ldr     r2, [r9, #offObject_clazz]  @ r2<- thisPtr->clazz
    ldr     r2, [r2, #offClassObject_vtable]    @ r2<- thisPtr->clazz->vtable
    EXPORT_PC()                         @ invoke must export
    ldr     r0, [r2, r1, lsl #2]        @ r3<- vtable[BBBB]
    bl      common_invokeMethodNoRange @ (r0=method, r9="this")

/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_VIRTUAL_QUICK_RANGE: /* 0xf9 */
/* File: armv5te/OP_INVOKE_VIRTUAL_QUICK_RANGE.S */
/* File: armv5te/OP_INVOKE_VIRTUAL_QUICK.S */
    /*
     * Handle an optimized virtual method call.
     *
     * for: [opt] invoke-virtual-quick, invoke-virtual-quick/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(r3, 2)                        @ r3<- FEDC or CCCC
    FETCH(r1, 1)                        @ r1<- BBBB
    .if     (!1)
    and     r3, r3, #15                 @ r3<- C (or stays CCCC)
    .endif
    GET_VREG(r9, r3)                    @ r9<- vC ("this" ptr)
    cmp     r9, #0                      @ is "this" null?
    beq     common_errNullObject        @ null "this", throw exception
    ldr     r2, [r9, #offObject_clazz]  @ r2<- thisPtr->clazz
    ldr     r2, [r2, #offClassObject_vtable]    @ r2<- thisPtr->clazz->vtable
    EXPORT_PC()                         @ invoke must export
    ldr     r0, [r2, r1, lsl #2]        @ r3<- vtable[BBBB]
    bl      common_invokeMethodRange @ (r0=method, r9="this")


/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_SUPER_QUICK: /* 0xfa */
/* File: armv5te/OP_INVOKE_SUPER_QUICK.S */
    /*
     * Handle an optimized "super" method call.
     *
     * for: [opt] invoke-super-quick, invoke-super-quick/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(r10, 2)                       @ r10<- GFED or CCCC
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    .if     (!0)
    and     r10, r10, #15               @ r10<- D (or stays CCCC)
    .endif
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r2, [r2, #offMethod_clazz]  @ r2<- method->clazz
    EXPORT_PC()                         @ must export for invoke
    ldr     r2, [r2, #offClassObject_super]     @ r2<- method->clazz->super
    GET_VREG(r9, r10)                   @ r9<- "this"
    ldr     r2, [r2, #offClassObject_vtable]    @ r2<- ...clazz->super->vtable
    cmp     r9, #0                      @ null "this" ref?
    ldr     r0, [r2, r1, lsl #2]        @ r0<- super->vtable[BBBB]
    beq     common_errNullObject        @ "this" is null, throw exception
    bl      common_invokeMethodNoRange @ (r0=method, r9="this")

/* ------------------------------ */
    .balign 64
.L_OP_INVOKE_SUPER_QUICK_RANGE: /* 0xfb */
/* File: armv5te/OP_INVOKE_SUPER_QUICK_RANGE.S */
/* File: armv5te/OP_INVOKE_SUPER_QUICK.S */
    /*
     * Handle an optimized "super" method call.
     *
     * for: [opt] invoke-super-quick, invoke-super-quick/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(r10, 2)                       @ r10<- GFED or CCCC
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    .if     (!1)
    and     r10, r10, #15               @ r10<- D (or stays CCCC)
    .endif
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r2, [r2, #offMethod_clazz]  @ r2<- method->clazz
    EXPORT_PC()                         @ must export for invoke
    ldr     r2, [r2, #offClassObject_super]     @ r2<- method->clazz->super
    GET_VREG(r9, r10)                   @ r9<- "this"
    ldr     r2, [r2, #offClassObject_vtable]    @ r2<- ...clazz->super->vtable
    cmp     r9, #0                      @ null "this" ref?
    ldr     r0, [r2, r1, lsl #2]        @ r0<- super->vtable[BBBB]
    beq     common_errNullObject        @ "this" is null, throw exception
    bl      common_invokeMethodRange @ (r0=method, r9="this")


/* ------------------------------ */
    .balign 64
.L_OP_IPUT_OBJECT_VOLATILE: /* 0xfc */
/* File: armv5te/OP_IPUT_OBJECT_VOLATILE.S */
/* File: armv5te/OP_IPUT_OBJECT.S */
    /*
     * 32-bit instance field put.
     *
     * for: iput-object, iput-object-volatile
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .LOP_IPUT_OBJECT_VOLATILE_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ success?
    bne     .LOP_IPUT_OBJECT_VOLATILE_finish          @ yes, finish up
    b       common_exceptionThrown


/* ------------------------------ */
    .balign 64
.L_OP_SGET_OBJECT_VOLATILE: /* 0xfd */
/* File: armv5te/OP_SGET_OBJECT_VOLATILE.S */
/* File: armv5te/OP_SGET.S */
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SGET_OBJECT_VOLATILE_resolve         @ yes, do resolve
.LOP_SGET_OBJECT_VOLATILE_finish: @ field ptr in r0
    ldr     r1, [r0, #offStaticField_value] @ r1<- field value
    SMP_DMB                            @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r2)                    @ fp[AA]<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction


/* ------------------------------ */
    .balign 64
.L_OP_SPUT_OBJECT_VOLATILE: /* 0xfe */
/* File: armv5te/OP_SPUT_OBJECT_VOLATILE.S */
/* File: armv5te/OP_SPUT_OBJECT.S */
    /*
     * 32-bit SPUT handler for objects
     *
     * for: sput-object, sput-object-volatile
     */
    /* op vAA, field@BBBB */
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref BBBB
    ldr     r10, [r2, #offDvmDex_pResFields] @ r10<- dvmDex->pResFields
    ldr     r0, [r10, r1, lsl #2]        @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ is resolved entry null?
    beq     .LOP_SPUT_OBJECT_VOLATILE_resolve         @ yes, do resolve
.LOP_SPUT_OBJECT_VOLATILE_finish:   @ field ptr in r0
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r1, r2)                    @ r1<- fp[AA]
    ldr     r2, [rSELF, #offThread_cardTable]  @ r2<- card table base
    ldr     r9, [r0, #offField_clazz]   @ r9<- field->clazz
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SMP_DMB_ST                        @ releasing store
    b       .LOP_SPUT_OBJECT_VOLATILE_end


/* ------------------------------ */
    .balign 64
.L_OP_UNUSED_FF: /* 0xff */
/* File: armv5te/OP_UNUSED_FF.S */
/* File: armv5te/unused.S */
    bl      common_abort


    .balign 64
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

/* continuation for OP_CONST_STRING */

    /*
     * Continuation if the String has not yet been resolved.
     *  r1: BBBB (String ref)
     *  r9: target register
     */
.LOP_CONST_STRING_resolve:
    EXPORT_PC()
    ldr     r0, [rSELF, #offThread_method] @ r0<- self->method
    ldr     r0, [r0, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveString            @ r0<- String reference
    cmp     r0, #0                      @ failed?
    beq     common_exceptionThrown      @ yup, handle the exception
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_CONST_STRING_JUMBO */

    /*
     * Continuation if the String has not yet been resolved.
     *  r1: BBBBBBBB (String ref)
     *  r9: target register
     */
.LOP_CONST_STRING_JUMBO_resolve:
    EXPORT_PC()
    ldr     r0, [rSELF, #offThread_method] @ r0<- self->method
    ldr     r0, [r0, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveString            @ r0<- String reference
    cmp     r0, #0                      @ failed?
    beq     common_exceptionThrown      @ yup, handle the exception
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_CONST_CLASS */

    /*
     * Continuation if the Class has not yet been resolved.
     *  r1: BBBB (Class ref)
     *  r9: target register
     */
.LOP_CONST_CLASS_resolve:
    EXPORT_PC()
    ldr     r0, [rSELF, #offThread_method] @ r0<- self->method
    mov     r2, #1                      @ r2<- true
    ldr     r0, [r0, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveClass             @ r0<- Class reference
    cmp     r0, #0                      @ failed?
    beq     common_exceptionThrown      @ yup, handle the exception
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_CHECK_CAST */

    /*
     * Trivial test failed, need to perform full check.  This is common.
     *  r0 holds obj->clazz
     *  r1 holds desired class resolved from BBBB
     *  r9 holds object
     */
.LOP_CHECK_CAST_fullcheck:
    mov     r10, r1                     @ avoid ClassObject getting clobbered
    bl      dvmInstanceofNonTrivial     @ r0<- boolean result
    cmp     r0, #0                      @ failed?
    bne     .LOP_CHECK_CAST_okay            @ no, success

    @ A cast has failed.  We need to throw a ClassCastException.
    EXPORT_PC()                         @ about to throw
    ldr     r0, [r9, #offObject_clazz]  @ r0<- obj->clazz (actual class)
    mov     r1, r10                     @ r1<- desired class
    bl      dvmThrowClassCastException
    b       common_exceptionThrown

    /*
     * Resolution required.  This is the least-likely path.
     *
     *  r2 holds BBBB
     *  r9 holds object
     */
.LOP_CHECK_CAST_resolve:
    EXPORT_PC()                         @ resolve() could throw
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    mov     r1, r2                      @ r1<- BBBB
    mov     r2, #0                      @ r2<- false
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveClass             @ r0<- resolved ClassObject ptr
    cmp     r0, #0                      @ got null?
    beq     common_exceptionThrown      @ yes, handle exception
    mov     r1, r0                      @ r1<- class resolved from BBB
    ldr     r0, [r9, #offObject_clazz]  @ r0<- obj->clazz
    b       .LOP_CHECK_CAST_resolved        @ pick up where we left off

/* continuation for OP_INSTANCE_OF */

    /*
     * Trivial test failed, need to perform full check.  This is common.
     *  r0 holds obj->clazz
     *  r1 holds class resolved from BBBB
     *  r9 holds A
     */
.LOP_INSTANCE_OF_fullcheck:
    bl      dvmInstanceofNonTrivial     @ r0<- boolean result
    @ fall through to OP_INSTANCE_OF_store

    /*
     * r0 holds boolean result
     * r9 holds A
     */
.LOP_INSTANCE_OF_store:
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r9)                    @ vA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

    /*
     * Trivial test succeeded, save and bail.
     *  r9 holds A
     */
.LOP_INSTANCE_OF_trivial:
    mov     r0, #1                      @ indicate success
    @ could b OP_INSTANCE_OF_store, but copying is faster and cheaper
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r9)                    @ vA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

    /*
     * Resolution required.  This is the least-likely path.
     *
     *  r3 holds BBBB
     *  r9 holds A
     */
.LOP_INSTANCE_OF_resolve:
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [rSELF, #offThread_method]    @ r0<- self->method
    mov     r1, r3                      @ r1<- BBBB
    mov     r2, #1                      @ r2<- true
    ldr     r0, [r0, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveClass             @ r0<- resolved ClassObject ptr
    cmp     r0, #0                      @ got null?
    beq     common_exceptionThrown      @ yes, handle exception
    mov     r1, r0                      @ r1<- class resolved from BBB
    mov     r3, rINST, lsr #12          @ r3<- B
    GET_VREG(r0, r3)                    @ r0<- vB (object)
    ldr     r0, [r0, #offObject_clazz]  @ r0<- obj->clazz
    b       .LOP_INSTANCE_OF_resolved        @ pick up where we left off

/* continuation for OP_NEW_INSTANCE */

    .balign 32                          @ minimize cache lines
.LOP_NEW_INSTANCE_finish: @ r0=new object
    mov     r3, rINST, lsr #8           @ r3<- AA
    cmp     r0, #0                      @ failed?
#if defined(WITH_JIT)
    /*
     * The JIT needs the class to be fully resolved before it can
     * include this instruction in a trace.
     */
    ldrh    r1, [rSELF, #offThread_subMode]
    beq     common_exceptionThrown      @ yes, handle the exception
    ands    r1, #kSubModeJitTraceBuild  @ under construction?
    bne     .LOP_NEW_INSTANCE_jitCheck
#else
    beq     common_exceptionThrown      @ yes, handle the exception
#endif
.LOP_NEW_INSTANCE_end:
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r3)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

#if defined(WITH_JIT)
    /*
     * Check to see if we need to stop the trace building early.
     * r0: new object
     * r3: vAA
     */
.LOP_NEW_INSTANCE_jitCheck:
    ldr     r1, [r10]                   @ reload resolved class
    cmp     r1, #0                      @ okay?
    bne     .LOP_NEW_INSTANCE_end             @ yes, finish
    mov     r9, r0                      @ preserve new object
    mov     r10, r3                     @ preserve vAA
    mov     r0, rSELF
    mov     r1, rPC
    bl      dvmJitEndTraceSelect        @ (self, pc)
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r9, r10)                   @ vAA<- new object
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
#endif

    /*
     * Class initialization required.
     *
     *  r0 holds class object
     */
.LOP_NEW_INSTANCE_needinit:
    mov     r9, r0                      @ save r0
    bl      dvmInitClass                @ initialize class
    cmp     r0, #0                      @ check boolean result
    mov     r0, r9                      @ restore r0
    bne     .LOP_NEW_INSTANCE_initialized     @ success, continue
    b       common_exceptionThrown      @ failed, deal with init exception

    /*
     * Resolution required.  This is the least-likely path.
     *
     *  r1 holds BBBB
     */
.LOP_NEW_INSTANCE_resolve:
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    mov     r2, #0                      @ r2<- false
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveClass             @ r0<- resolved ClassObject ptr
    cmp     r0, #0                      @ got null?
    bne     .LOP_NEW_INSTANCE_resolved        @ no, continue
    b       common_exceptionThrown      @ yes, handle exception

/* continuation for OP_NEW_ARRAY */


    /*
     * Resolve class.  (This is an uncommon case.)
     *
     *  r1 holds array length
     *  r2 holds class ref CCCC
     */
.LOP_NEW_ARRAY_resolve:
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    mov     r9, r1                      @ r9<- length (save)
    mov     r1, r2                      @ r1<- CCCC
    mov     r2, #0                      @ r2<- false
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveClass             @ r0<- call(clazz, ref)
    cmp     r0, #0                      @ got null?
    mov     r1, r9                      @ r1<- length (restore)
    beq     common_exceptionThrown      @ yes, handle exception
    @ fall through to OP_NEW_ARRAY_finish

    /*
     * Finish allocation.
     *
     *  r0 holds class
     *  r1 holds array length
     */
.LOP_NEW_ARRAY_finish:
    mov     r2, #ALLOC_DONT_TRACK       @ don't track in local refs table
    bl      dvmAllocArrayByClass        @ r0<- call(clazz, length, flags)
    cmp     r0, #0                      @ failed?
    mov     r2, rINST, lsr #8           @ r2<- A+
    beq     common_exceptionThrown      @ yes, handle the exception
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15                 @ r2<- A
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ vA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_FILLED_NEW_ARRAY */

    /*
     * On entry:
     *  r0 holds array class
     *  r10 holds AA or BA
     */
.LOP_FILLED_NEW_ARRAY_continue:
    ldr     r3, [r0, #offClassObject_descriptor] @ r3<- arrayClass->descriptor
    mov     r2, #ALLOC_DONT_TRACK       @ r2<- alloc flags
    ldrb    rINST, [r3, #1]             @ rINST<- descriptor[1]
    .if     0
    mov     r1, r10                     @ r1<- AA (length)
    .else
    mov     r1, r10, lsr #4             @ r1<- B (length)
    .endif
    cmp     rINST, #'I'                 @ array of ints?
    cmpne   rINST, #'L'                 @ array of objects?
    cmpne   rINST, #'['                 @ array of arrays?
    mov     r9, r1                      @ save length in r9
    bne     .LOP_FILLED_NEW_ARRAY_notimpl         @ no, not handled yet
    bl      dvmAllocArrayByClass        @ r0<- call(arClass, length, flags)
    cmp     r0, #0                      @ null return?
    beq     common_exceptionThrown      @ alloc failed, handle exception

    FETCH(r1, 2)                        @ r1<- FEDC or CCCC
    str     r0, [rSELF, #offThread_retval]      @ retval.l <- new array
    str     rINST, [rSELF, #offThread_retval+4] @ retval.h <- type
    add     r0, r0, #offArrayObject_contents @ r0<- newArray->contents
    subs    r9, r9, #1                  @ length--, check for neg
    FETCH_ADVANCE_INST(3)               @ advance to next instr, load rINST
    bmi     2f                          @ was zero, bail

    @ copy values from registers into the array
    @ r0=array, r1=CCCC/FEDC, r9=length (from AA or B), r10=AA/BA
    .if     0
    add     r2, rFP, r1, lsl #2         @ r2<- &fp[CCCC]
1:  ldr     r3, [r2], #4                @ r3<- *r2++
    subs    r9, r9, #1                  @ count--
    str     r3, [r0], #4                @ *contents++ = vX
    bpl     1b
    @ continue at 2
    .else
    cmp     r9, #4                      @ length was initially 5?
    and     r2, r10, #15                @ r2<- A
    bne     1f                          @ <= 4 args, branch
    GET_VREG(r3, r2)                    @ r3<- vA
    sub     r9, r9, #1                  @ count--
    str     r3, [r0, #16]               @ contents[4] = vA
1:  and     r2, r1, #15                 @ r2<- F/E/D/C
    GET_VREG(r3, r2)                    @ r3<- vF/vE/vD/vC
    mov     r1, r1, lsr #4              @ r1<- next reg in low 4
    subs    r9, r9, #1                  @ count--
    str     r3, [r0], #4                @ *contents++ = vX
    bpl     1b
    @ continue at 2
    .endif

2:
    ldr     r0, [rSELF, #offThread_retval]     @ r0<- object
    ldr     r1, [rSELF, #offThread_retval+4]   @ r1<- type
    ldr     r2, [rSELF, #offThread_cardTable]  @ r2<- card table base
    GET_INST_OPCODE(ip)                      @ ip<- opcode from rINST
    cmp     r1, #'I'                         @ Is int array?
    strneb  r2, [r2, r0, lsr #GC_CARD_SHIFT] @ Mark card based on object head
    GOTO_OPCODE(ip)                          @ execute it

    /*
     * Throw an exception indicating that we have not implemented this
     * mode of filled-new-array.
     */
.LOP_FILLED_NEW_ARRAY_notimpl:
    ldr     r0, .L_strFilledNewArrayNotImpl_OP_FILLED_NEW_ARRAY
3:  add     r0, pc
    bl      dvmThrowInternalError
    b       common_exceptionThrown

    /*
     * Ideally we'd only define this once, but depending on layout we can
     * exceed the range of the load above.
     */

.L_strFilledNewArrayNotImpl_OP_FILLED_NEW_ARRAY:
    .word   PCREL_REF(.LstrFilledNewArrayNotImpl,3b)

/* continuation for OP_FILLED_NEW_ARRAY_RANGE */

    /*
     * On entry:
     *  r0 holds array class
     *  r10 holds AA or BA
     */
.LOP_FILLED_NEW_ARRAY_RANGE_continue:
    ldr     r3, [r0, #offClassObject_descriptor] @ r3<- arrayClass->descriptor
    mov     r2, #ALLOC_DONT_TRACK       @ r2<- alloc flags
    ldrb    rINST, [r3, #1]             @ rINST<- descriptor[1]
    .if     1
    mov     r1, r10                     @ r1<- AA (length)
    .else
    mov     r1, r10, lsr #4             @ r1<- B (length)
    .endif
    cmp     rINST, #'I'                 @ array of ints?
    cmpne   rINST, #'L'                 @ array of objects?
    cmpne   rINST, #'['                 @ array of arrays?
    mov     r9, r1                      @ save length in r9
    bne     .LOP_FILLED_NEW_ARRAY_RANGE_notimpl         @ no, not handled yet
    bl      dvmAllocArrayByClass        @ r0<- call(arClass, length, flags)
    cmp     r0, #0                      @ null return?
    beq     common_exceptionThrown      @ alloc failed, handle exception

    FETCH(r1, 2)                        @ r1<- FEDC or CCCC
    str     r0, [rSELF, #offThread_retval]      @ retval.l <- new array
    str     rINST, [rSELF, #offThread_retval+4] @ retval.h <- type
    add     r0, r0, #offArrayObject_contents @ r0<- newArray->contents
    subs    r9, r9, #1                  @ length--, check for neg
    FETCH_ADVANCE_INST(3)               @ advance to next instr, load rINST
    bmi     2f                          @ was zero, bail

    @ copy values from registers into the array
    @ r0=array, r1=CCCC/FEDC, r9=length (from AA or B), r10=AA/BA
    .if     1
    add     r2, rFP, r1, lsl #2         @ r2<- &fp[CCCC]
1:  ldr     r3, [r2], #4                @ r3<- *r2++
    subs    r9, r9, #1                  @ count--
    str     r3, [r0], #4                @ *contents++ = vX
    bpl     1b
    @ continue at 2
    .else
    cmp     r9, #4                      @ length was initially 5?
    and     r2, r10, #15                @ r2<- A
    bne     1f                          @ <= 4 args, branch
    GET_VREG(r3, r2)                    @ r3<- vA
    sub     r9, r9, #1                  @ count--
    str     r3, [r0, #16]               @ contents[4] = vA
1:  and     r2, r1, #15                 @ r2<- F/E/D/C
    GET_VREG(r3, r2)                    @ r3<- vF/vE/vD/vC
    mov     r1, r1, lsr #4              @ r1<- next reg in low 4
    subs    r9, r9, #1                  @ count--
    str     r3, [r0], #4                @ *contents++ = vX
    bpl     1b
    @ continue at 2
    .endif

2:
    ldr     r0, [rSELF, #offThread_retval]     @ r0<- object
    ldr     r1, [rSELF, #offThread_retval+4]   @ r1<- type
    ldr     r2, [rSELF, #offThread_cardTable]  @ r2<- card table base
    GET_INST_OPCODE(ip)                      @ ip<- opcode from rINST
    cmp     r1, #'I'                         @ Is int array?
    strneb  r2, [r2, r0, lsr #GC_CARD_SHIFT] @ Mark card based on object head
    GOTO_OPCODE(ip)                          @ execute it

    /*
     * Throw an exception indicating that we have not implemented this
     * mode of filled-new-array.
     */
.LOP_FILLED_NEW_ARRAY_RANGE_notimpl:
    ldr     r0, .L_strFilledNewArrayNotImpl_OP_FILLED_NEW_ARRAY_RANGE
3:  add     r0, pc
    bl      dvmThrowInternalError
    b       common_exceptionThrown

    /*
     * Ideally we'd only define this once, but depending on layout we can
     * exceed the range of the load above.
     */

.L_strFilledNewArrayNotImpl_OP_FILLED_NEW_ARRAY_RANGE:
    .word   PCREL_REF(.LstrFilledNewArrayNotImpl,3b)

/* continuation for OP_CMPL_FLOAT */
.LOP_CMPL_FLOAT_finish:
    SET_VREG(r0, r9)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_CMPG_FLOAT */
.LOP_CMPG_FLOAT_finish:
    SET_VREG(r0, r9)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_CMPL_DOUBLE */
.LOP_CMPL_DOUBLE_finish:
    SET_VREG(r0, r9)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_CMPG_DOUBLE */
.LOP_CMPG_DOUBLE_finish:
    SET_VREG(r0, r9)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_CMP_LONG */

.LOP_CMP_LONG_less:
    mvn     r1, #0                      @ r1<- -1
    @ Want to cond code the next mov so we can avoid branch, but don't see it;
    @ instead, we just replicate the tail end.
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r9)                    @ vAA<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

.LOP_CMP_LONG_greater:
    mov     r1, #1                      @ r1<- 1
    @ fall through to _finish

.LOP_CMP_LONG_finish:
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r9)                    @ vAA<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_AGET_WIDE */

.LOP_AGET_WIDE_finish:
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldrd    r2, [r0, #offArrayObject_contents]  @ r2/r3<- vBB[vCC]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r2-r3}                 @ vAA/vAA+1<- r2/r3
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_APUT_WIDE */

.LOP_APUT_WIDE_finish:
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldmia   r9, {r2-r3}                 @ r2/r3<- vAA/vAA+1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    strd    r2, [r0, #offArrayObject_contents]  @ r2/r3<- vBB[vCC]
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_APUT_OBJECT */
    /*
     * On entry:
     *  rINST = vBB (arrayObj)
     *  r9 = vAA (obj)
     *  r10 = offset into array (vBB + vCC * width)
     */
.LOP_APUT_OBJECT_finish:
    cmp     r9, #0                      @ storing null reference?
    beq     .LOP_APUT_OBJECT_skip_check      @ yes, skip type checks
    ldr     r0, [r9, #offObject_clazz]  @ r0<- obj->clazz
    ldr     r1, [rINST, #offObject_clazz]  @ r1<- arrayObj->clazz
    bl      dvmCanPutArrayElement       @ test object type vs. array type
    cmp     r0, #0                      @ okay?
    beq     .LOP_APUT_OBJECT_throw           @ no
    mov     r1, rINST                   @ r1<- arrayObj
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldr     r2, [rSELF, #offThread_cardTable]     @ get biased CT base
    add     r10, #offArrayObject_contents   @ r0<- pointer to slot
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    str     r9, [r10]                   @ vBB[vCC]<- vAA
    strb    r2, [r2, r1, lsr #GC_CARD_SHIFT] @ mark card using object head
    GOTO_OPCODE(ip)                     @ jump to next instruction
.LOP_APUT_OBJECT_skip_check:
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    str     r9, [r10, #offArrayObject_contents] @ vBB[vCC]<- vAA
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
.LOP_APUT_OBJECT_throw:
    @ The types don't match.  We need to throw an ArrayStoreException.
    ldr     r0, [r9, #offObject_clazz]
    ldr     r1, [rINST, #offObject_clazz]
    EXPORT_PC()
    bl      dvmThrowArrayStoreExceptionIncompatibleElement
    b       common_exceptionThrown

/* continuation for OP_IGET */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IGET_finish:
    @bl      common_squeak0
    cmp     r9, #0                      @ check object for null
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    beq     common_errNullObject        @ object was null
    ldr   r0, [r9, r3]                @ r0<- obj.field (8/16/32 bits)
    @ no-op                             @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- A+
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15                 @ r2<- A
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[A]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IGET_WIDE */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IGET_WIDE_finish:
    cmp     r9, #0                      @ check object for null
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    beq     common_errNullObject        @ object was null
    .if     0
    add     r0, r9, r3                  @ r0<- address of field
    bl      dvmQuasiAtomicRead64        @ r0/r1<- contents of field
    .else
    ldrd    r0, [r9, r3]                @ r0/r1<- obj.field (64-bit align ok)
    .endif
    mov     r2, rINST, lsr #8           @ r2<- A+
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15                 @ r2<- A
    add     r3, rFP, r2, lsl #2         @ r3<- &fp[A]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r3, {r0-r1}                 @ fp[A]<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IGET_OBJECT */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IGET_OBJECT_finish:
    @bl      common_squeak0
    cmp     r9, #0                      @ check object for null
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    beq     common_errNullObject        @ object was null
    ldr   r0, [r9, r3]                @ r0<- obj.field (8/16/32 bits)
    @ no-op                             @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- A+
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15                 @ r2<- A
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[A]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IGET_BOOLEAN */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IGET_BOOLEAN_finish:
    @bl      common_squeak1
    cmp     r9, #0                      @ check object for null
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    beq     common_errNullObject        @ object was null
    ldr   r0, [r9, r3]                @ r0<- obj.field (8/16/32 bits)
    @ no-op                             @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- A+
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15                 @ r2<- A
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[A]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IGET_BYTE */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IGET_BYTE_finish:
    @bl      common_squeak2
    cmp     r9, #0                      @ check object for null
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    beq     common_errNullObject        @ object was null
    ldr   r0, [r9, r3]                @ r0<- obj.field (8/16/32 bits)
    @ no-op                             @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- A+
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15                 @ r2<- A
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[A]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IGET_CHAR */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IGET_CHAR_finish:
    @bl      common_squeak3
    cmp     r9, #0                      @ check object for null
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    beq     common_errNullObject        @ object was null
    ldr   r0, [r9, r3]                @ r0<- obj.field (8/16/32 bits)
    @ no-op                             @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- A+
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15                 @ r2<- A
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[A]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IGET_SHORT */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IGET_SHORT_finish:
    @bl      common_squeak4
    cmp     r9, #0                      @ check object for null
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    beq     common_errNullObject        @ object was null
    ldr   r0, [r9, r3]                @ r0<- obj.field (8/16/32 bits)
    @ no-op                             @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- A+
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15                 @ r2<- A
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[A]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IPUT */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IPUT_finish:
    @bl      common_squeak0
    mov     r1, rINST, lsr #8           @ r1<- A+
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    and     r1, r1, #15                 @ r1<- A
    cmp     r9, #0                      @ check object for null
    GET_VREG(r0, r1)                    @ r0<- fp[A]
    beq     common_errNullObject        @ object was null
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    @ no-op                         @ releasing store
    str  r0, [r9, r3]                @ obj.field (8/16/32 bits)<- r0
    @ no-op 
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IPUT_WIDE */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IPUT_WIDE_finish:
    mov     r2, rINST, lsr #8           @ r2<- A+
    cmp     r9, #0                      @ check object for null
    and     r2, r2, #15                 @ r2<- A
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    add     r2, rFP, r2, lsl #2         @ r3<- &fp[A]
    beq     common_errNullObject        @ object was null
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldmia   r2, {r0-r1}                 @ r0/r1<- fp[A]
    GET_INST_OPCODE(r10)                @ extract opcode from rINST
    .if     0
    add     r2, r9, r3                  @ r2<- target address
    bl      dvmQuasiAtomicSwap64Sync    @ stores r0/r1 into addr r2
    .else
    strd    r0, [r9, r3]                @ obj.field (64 bits, aligned)<- r0/r1
    .endif
    GOTO_OPCODE(r10)                    @ jump to next instruction

/* continuation for OP_IPUT_OBJECT */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IPUT_OBJECT_finish:
    @bl      common_squeak0
    mov     r1, rINST, lsr #8           @ r1<- A+
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    and     r1, r1, #15                 @ r1<- A
    cmp     r9, #0                      @ check object for null
    GET_VREG(r0, r1)                    @ r0<- fp[A]
    ldr     r2, [rSELF, #offThread_cardTable]  @ r2<- card table base
    beq     common_errNullObject        @ object was null
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    @ no-op                         @ releasing store
    str     r0, [r9, r3]                @ obj.field (32 bits)<- r0
    @ no-op 
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    cmp     r0, #0                      @ stored a null reference?
    strneb  r2, [r2, r9, lsr #GC_CARD_SHIFT]  @ mark card if not
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IPUT_BOOLEAN */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IPUT_BOOLEAN_finish:
    @bl      common_squeak1
    mov     r1, rINST, lsr #8           @ r1<- A+
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    and     r1, r1, #15                 @ r1<- A
    cmp     r9, #0                      @ check object for null
    GET_VREG(r0, r1)                    @ r0<- fp[A]
    beq     common_errNullObject        @ object was null
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    @ no-op                         @ releasing store
    str  r0, [r9, r3]                @ obj.field (8/16/32 bits)<- r0
    @ no-op 
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IPUT_BYTE */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IPUT_BYTE_finish:
    @bl      common_squeak2
    mov     r1, rINST, lsr #8           @ r1<- A+
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    and     r1, r1, #15                 @ r1<- A
    cmp     r9, #0                      @ check object for null
    GET_VREG(r0, r1)                    @ r0<- fp[A]
    beq     common_errNullObject        @ object was null
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    @ no-op                         @ releasing store
    str  r0, [r9, r3]                @ obj.field (8/16/32 bits)<- r0
    @ no-op 
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IPUT_CHAR */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IPUT_CHAR_finish:
    @bl      common_squeak3
    mov     r1, rINST, lsr #8           @ r1<- A+
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    and     r1, r1, #15                 @ r1<- A
    cmp     r9, #0                      @ check object for null
    GET_VREG(r0, r1)                    @ r0<- fp[A]
    beq     common_errNullObject        @ object was null
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    @ no-op                         @ releasing store
    str  r0, [r9, r3]                @ obj.field (8/16/32 bits)<- r0
    @ no-op 
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IPUT_SHORT */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IPUT_SHORT_finish:
    @bl      common_squeak4
    mov     r1, rINST, lsr #8           @ r1<- A+
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    and     r1, r1, #15                 @ r1<- A
    cmp     r9, #0                      @ check object for null
    GET_VREG(r0, r1)                    @ r0<- fp[A]
    beq     common_errNullObject        @ object was null
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    @ no-op                         @ releasing store
    str  r0, [r9, r3]                @ obj.field (8/16/32 bits)<- r0
    @ no-op 
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_SGET */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SGET_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SGET_finish

/* continuation for OP_SGET_WIDE */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     *
     * Returns StaticField pointer in r0.
     */
.LOP_SGET_WIDE_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r1<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SGET_WIDE_finish          @ resume

/* continuation for OP_SGET_OBJECT */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SGET_OBJECT_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SGET_OBJECT_finish

/* continuation for OP_SGET_BOOLEAN */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SGET_BOOLEAN_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SGET_BOOLEAN_finish

/* continuation for OP_SGET_BYTE */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SGET_BYTE_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SGET_BYTE_finish

/* continuation for OP_SGET_CHAR */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SGET_CHAR_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SGET_CHAR_finish

/* continuation for OP_SGET_SHORT */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SGET_SHORT_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SGET_SHORT_finish

/* continuation for OP_SPUT */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SPUT_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SPUT_finish          @ resume

/* continuation for OP_SPUT_WIDE */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r9:  &fp[AA]
     *  r10: dvmDex->pResFields
     *
     * Returns StaticField pointer in r2.
     */
.LOP_SPUT_WIDE_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    mov     r2, r0                      @ copy to r2
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SPUT_WIDE_finish          @ resume

/* continuation for OP_SPUT_OBJECT */


.LOP_SPUT_OBJECT_end:
    str     r1, [r0, #offStaticField_value]  @ field<- vAA
    @ no-op 
    cmp     r1, #0                      @ stored a null object?
    strneb  r2, [r2, r9, lsr #GC_CARD_SHIFT]  @ mark card based on obj head
    GOTO_OPCODE(ip)                     @ jump to next instruction

    /* Continuation if the field has not yet been resolved.
     * r1:  BBBB field ref
     * r10: dvmDex->pResFields
     */
.LOP_SPUT_OBJECT_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SPUT_OBJECT_finish          @ resume


/* continuation for OP_SPUT_BOOLEAN */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SPUT_BOOLEAN_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SPUT_BOOLEAN_finish          @ resume

/* continuation for OP_SPUT_BYTE */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SPUT_BYTE_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SPUT_BYTE_finish          @ resume

/* continuation for OP_SPUT_CHAR */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SPUT_CHAR_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SPUT_CHAR_finish          @ resume

/* continuation for OP_SPUT_SHORT */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SPUT_SHORT_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SPUT_SHORT_finish          @ resume

/* continuation for OP_INVOKE_VIRTUAL */

    /*
     * At this point:
     *  r0 = resolved base method
     *  r10 = C or CCCC (index of first arg, which is the "this" ptr)
     */
.LOP_INVOKE_VIRTUAL_continue:
    GET_VREG(r9, r10)                   @ r9<- "this" ptr
    ldrh    r2, [r0, #offMethod_methodIndex]    @ r2<- baseMethod->methodIndex
    cmp     r9, #0                      @ is "this" null?
    beq     common_errNullObject        @ null "this", throw exception
    ldr     r3, [r9, #offObject_clazz]  @ r3<- thisPtr->clazz
    ldr     r3, [r3, #offClassObject_vtable]    @ r3<- thisPtr->clazz->vtable
    ldr     r0, [r3, r2, lsl #2]        @ r3<- vtable[methodIndex]
    bl      common_invokeMethodNoRange @ (r0=method, r9="this")

/* continuation for OP_INVOKE_SUPER */

    /*
     * At this point:
     *  r0 = resolved base method
     *  r10 = method->clazz
     */
.LOP_INVOKE_SUPER_continue:
    ldr     r1, [r10, #offClassObject_super]    @ r1<- method->clazz->super
    ldrh    r2, [r0, #offMethod_methodIndex]    @ r2<- baseMethod->methodIndex
    ldr     r3, [r1, #offClassObject_vtableCount]   @ r3<- super->vtableCount
    EXPORT_PC()                         @ must export for invoke
    cmp     r2, r3                      @ compare (methodIndex, vtableCount)
    bcs     .LOP_INVOKE_SUPER_nsm             @ method not present in superclass
    ldr     r1, [r1, #offClassObject_vtable]    @ r1<- ...clazz->super->vtable
    ldr     r0, [r1, r2, lsl #2]        @ r3<- vtable[methodIndex]
    bl      common_invokeMethodNoRange @ continue on

.LOP_INVOKE_SUPER_resolve:
    mov     r0, r10                     @ r0<- method->clazz
    mov     r2, #METHOD_VIRTUAL         @ resolver method type
    bl      dvmResolveMethod            @ r0<- call(clazz, ref, flags)
    cmp     r0, #0                      @ got null?
    bne     .LOP_INVOKE_SUPER_continue        @ no, continue
    b       common_exceptionThrown      @ yes, handle exception

    /*
     * Throw a NoSuchMethodError with the method name as the message.
     *  r0 = resolved base method
     */
.LOP_INVOKE_SUPER_nsm:
    ldr     r1, [r0, #offMethod_name]   @ r1<- method name
    b       common_errNoSuchMethod

/* continuation for OP_INVOKE_DIRECT */

    /*
     * On entry:
     *  r1 = reference (BBBB or CCCC)
     *  r10 = "this" register
     */
.LOP_INVOKE_DIRECT_resolve:
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    mov     r2, #METHOD_DIRECT          @ resolver method type
    bl      dvmResolveMethod            @ r0<- call(clazz, ref, flags)
    cmp     r0, #0                      @ got null?
    bne     .LOP_INVOKE_DIRECT_finish          @ no, continue
    b       common_exceptionThrown      @ yes, handle exception

/* continuation for OP_INVOKE_STATIC */


.LOP_INVOKE_STATIC_resolve:
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    mov     r2, #METHOD_STATIC          @ resolver method type
    bl      dvmResolveMethod            @ r0<- call(clazz, ref, flags)
    cmp     r0, #0                      @ got null?
#if defined(WITH_JIT)
    /*
     * Check to see if we're actively building a trace.  If so,
     * we need to keep this instruction out of it.
     * r10: &resolved_methodToCall
     */
    ldrh    r2, [rSELF, #offThread_subMode]
    beq     common_exceptionThrown            @ null, handle exception
    ands    r2, #kSubModeJitTraceBuild        @ trace under construction?
    beq     common_invokeMethodNoRange     @ no (r0=method, r9="this")
    ldr     r1, [r10]                         @ reload resolved method
    cmp     r1, #0                            @ finished resolving?
    bne     common_invokeMethodNoRange     @ yes (r0=method, r9="this")
    mov     r10, r0                           @ preserve method
    mov     r0, rSELF
    mov     r1, rPC
    bl      dvmJitEndTraceSelect              @ (self, pc)
    mov     r0, r10
    b       common_invokeMethodNoRange     @ whew, finally!
#else
    bne     common_invokeMethodNoRange     @ (r0=method, r9="this")
    b       common_exceptionThrown            @ yes, handle exception
#endif

/* continuation for OP_INVOKE_VIRTUAL_RANGE */

    /*
     * At this point:
     *  r0 = resolved base method
     *  r10 = C or CCCC (index of first arg, which is the "this" ptr)
     */
.LOP_INVOKE_VIRTUAL_RANGE_continue:
    GET_VREG(r9, r10)                   @ r9<- "this" ptr
    ldrh    r2, [r0, #offMethod_methodIndex]    @ r2<- baseMethod->methodIndex
    cmp     r9, #0                      @ is "this" null?
    beq     common_errNullObject        @ null "this", throw exception
    ldr     r3, [r9, #offObject_clazz]  @ r3<- thisPtr->clazz
    ldr     r3, [r3, #offClassObject_vtable]    @ r3<- thisPtr->clazz->vtable
    ldr     r0, [r3, r2, lsl #2]        @ r3<- vtable[methodIndex]
    bl      common_invokeMethodRange @ (r0=method, r9="this")

/* continuation for OP_INVOKE_SUPER_RANGE */

    /*
     * At this point:
     *  r0 = resolved base method
     *  r10 = method->clazz
     */
.LOP_INVOKE_SUPER_RANGE_continue:
    ldr     r1, [r10, #offClassObject_super]    @ r1<- method->clazz->super
    ldrh    r2, [r0, #offMethod_methodIndex]    @ r2<- baseMethod->methodIndex
    ldr     r3, [r1, #offClassObject_vtableCount]   @ r3<- super->vtableCount
    EXPORT_PC()                         @ must export for invoke
    cmp     r2, r3                      @ compare (methodIndex, vtableCount)
    bcs     .LOP_INVOKE_SUPER_RANGE_nsm             @ method not present in superclass
    ldr     r1, [r1, #offClassObject_vtable]    @ r1<- ...clazz->super->vtable
    ldr     r0, [r1, r2, lsl #2]        @ r3<- vtable[methodIndex]
    bl      common_invokeMethodRange @ continue on

.LOP_INVOKE_SUPER_RANGE_resolve:
    mov     r0, r10                     @ r0<- method->clazz
    mov     r2, #METHOD_VIRTUAL         @ resolver method type
    bl      dvmResolveMethod            @ r0<- call(clazz, ref, flags)
    cmp     r0, #0                      @ got null?
    bne     .LOP_INVOKE_SUPER_RANGE_continue        @ no, continue
    b       common_exceptionThrown      @ yes, handle exception

    /*
     * Throw a NoSuchMethodError with the method name as the message.
     *  r0 = resolved base method
     */
.LOP_INVOKE_SUPER_RANGE_nsm:
    ldr     r1, [r0, #offMethod_name]   @ r1<- method name
    b       common_errNoSuchMethod

/* continuation for OP_INVOKE_DIRECT_RANGE */

    /*
     * On entry:
     *  r1 = reference (BBBB or CCCC)
     *  r10 = "this" register
     */
.LOP_INVOKE_DIRECT_RANGE_resolve:
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    mov     r2, #METHOD_DIRECT          @ resolver method type
    bl      dvmResolveMethod            @ r0<- call(clazz, ref, flags)
    cmp     r0, #0                      @ got null?
    bne     .LOP_INVOKE_DIRECT_RANGE_finish          @ no, continue
    b       common_exceptionThrown      @ yes, handle exception

/* continuation for OP_INVOKE_STATIC_RANGE */


.LOP_INVOKE_STATIC_RANGE_resolve:
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    mov     r2, #METHOD_STATIC          @ resolver method type
    bl      dvmResolveMethod            @ r0<- call(clazz, ref, flags)
    cmp     r0, #0                      @ got null?
#if defined(WITH_JIT)
    /*
     * Check to see if we're actively building a trace.  If so,
     * we need to keep this instruction out of it.
     * r10: &resolved_methodToCall
     */
    ldrh    r2, [rSELF, #offThread_subMode]
    beq     common_exceptionThrown            @ null, handle exception
    ands    r2, #kSubModeJitTraceBuild        @ trace under construction?
    beq     common_invokeMethodRange     @ no (r0=method, r9="this")
    ldr     r1, [r10]                         @ reload resolved method
    cmp     r1, #0                            @ finished resolving?
    bne     common_invokeMethodRange     @ yes (r0=method, r9="this")
    mov     r10, r0                           @ preserve method
    mov     r0, rSELF
    mov     r1, rPC
    bl      dvmJitEndTraceSelect              @ (self, pc)
    mov     r0, r10
    b       common_invokeMethodRange     @ whew, finally!
#else
    bne     common_invokeMethodRange     @ (r0=method, r9="this")
    b       common_exceptionThrown            @ yes, handle exception
#endif

/* continuation for OP_FLOAT_TO_LONG */
/*
 * Convert the float in r0 to a long in r0/r1.
 *
 * We have to clip values to long min/max per the specification.  The
 * expected common case is a "reasonable" value that converts directly
 * to modest integer.  The EABI convert function isn't doing this for us.
 */
f2l_doconv:
    stmfd   sp!, {r4, lr}
    mov     r1, #0x5f000000             @ (float)maxlong
    mov     r4, r0
    bl      __aeabi_fcmpge              @ is arg >= maxlong?
    cmp     r0, #0                      @ nonzero == yes
    mvnne   r0, #0                      @ return maxlong (7fffffff)
    mvnne   r1, #0x80000000
    ldmnefd sp!, {r4, pc}

    mov     r0, r4                      @ recover arg
    mov     r1, #0xdf000000             @ (float)minlong
    bl      __aeabi_fcmple              @ is arg <= minlong?
    cmp     r0, #0                      @ nonzero == yes
    movne   r0, #0                      @ return minlong (80000000)
    movne   r1, #0x80000000
    ldmnefd sp!, {r4, pc}

    mov     r0, r4                      @ recover arg
    mov     r1, r4
    bl      __aeabi_fcmpeq              @ is arg == self?
    cmp     r0, #0                      @ zero == no
    moveq   r1, #0                      @ return zero for NaN
    ldmeqfd sp!, {r4, pc}

    mov     r0, r4                      @ recover arg
    bl      __aeabi_f2lz                @ convert float to long
    ldmfd   sp!, {r4, pc}

/* continuation for OP_DOUBLE_TO_LONG */
/*
 * Convert the double in r0/r1 to a long in r0/r1.
 *
 * We have to clip values to long min/max per the specification.  The
 * expected common case is a "reasonable" value that converts directly
 * to modest integer.  The EABI convert function isn't doing this for us.
 */
d2l_doconv:
    stmfd   sp!, {r4, r5, lr}           @ save regs
    mov     r3, #0x43000000             @ maxlong, as a double (high word)
    add     r3, #0x00e00000             @  0x43e00000
    mov     r2, #0                      @ maxlong, as a double (low word)
    sub     sp, sp, #4                  @ align for EABI
    mov     r4, r0                      @ save a copy of r0
    mov     r5, r1                      @  and r1
    bl      __aeabi_dcmpge              @ is arg >= maxlong?
    cmp     r0, #0                      @ nonzero == yes
    mvnne   r0, #0                      @ return maxlong (7fffffffffffffff)
    mvnne   r1, #0x80000000
    bne     1f

    mov     r0, r4                      @ recover arg
    mov     r1, r5
    mov     r3, #0xc3000000             @ minlong, as a double (high word)
    add     r3, #0x00e00000             @  0xc3e00000
    mov     r2, #0                      @ minlong, as a double (low word)
    bl      __aeabi_dcmple              @ is arg <= minlong?
    cmp     r0, #0                      @ nonzero == yes
    movne   r0, #0                      @ return minlong (8000000000000000)
    movne   r1, #0x80000000
    bne     1f

    mov     r0, r4                      @ recover arg
    mov     r1, r5
    mov     r2, r4                      @ compare against self
    mov     r3, r5
    bl      __aeabi_dcmpeq              @ is arg == self?
    cmp     r0, #0                      @ zero == no
    moveq   r1, #0                      @ return zero for NaN
    beq     1f

    mov     r0, r4                      @ recover arg
    mov     r1, r5
    bl      __aeabi_d2lz                @ convert double to long

1:
    add     sp, sp, #4
    ldmfd   sp!, {r4, r5, pc}

/* continuation for OP_MUL_LONG */

.LOP_MUL_LONG_finish:
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r0, {r9-r10}                @ vAA/vAA+1<- r9/r10
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_SHL_LONG */

.LOP_SHL_LONG_finish:
    mov     r0, r0, asl r2              @  r0<- r0 << r2
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_SHR_LONG */

.LOP_SHR_LONG_finish:
    mov     r1, r1, asr r2              @  r1<- r1 >> r2
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_USHR_LONG */

.LOP_USHR_LONG_finish:
    mov     r1, r1, lsr r2              @  r1<- r1 >>> r2
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_SHL_LONG_2ADDR */

.LOP_SHL_LONG_2ADDR_finish:
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_SHR_LONG_2ADDR */

.LOP_SHR_LONG_2ADDR_finish:
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_USHR_LONG_2ADDR */

.LOP_USHR_LONG_2ADDR_finish:
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IGET_VOLATILE */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IGET_VOLATILE_finish:
    @bl      common_squeak0
    cmp     r9, #0                      @ check object for null
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    beq     common_errNullObject        @ object was null
    ldr   r0, [r9, r3]                @ r0<- obj.field (8/16/32 bits)
    SMP_DMB                            @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- A+
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15                 @ r2<- A
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[A]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IPUT_VOLATILE */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IPUT_VOLATILE_finish:
    @bl      common_squeak0
    mov     r1, rINST, lsr #8           @ r1<- A+
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    and     r1, r1, #15                 @ r1<- A
    cmp     r9, #0                      @ check object for null
    GET_VREG(r0, r1)                    @ r0<- fp[A]
    beq     common_errNullObject        @ object was null
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SMP_DMB_ST                        @ releasing store
    str  r0, [r9, r3]                @ obj.field (8/16/32 bits)<- r0
    SMP_DMB
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_SGET_VOLATILE */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SGET_VOLATILE_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SGET_VOLATILE_finish

/* continuation for OP_SPUT_VOLATILE */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SPUT_VOLATILE_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SPUT_VOLATILE_finish          @ resume

/* continuation for OP_IGET_OBJECT_VOLATILE */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IGET_OBJECT_VOLATILE_finish:
    @bl      common_squeak0
    cmp     r9, #0                      @ check object for null
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    beq     common_errNullObject        @ object was null
    ldr   r0, [r9, r3]                @ r0<- obj.field (8/16/32 bits)
    SMP_DMB                            @ acquiring load
    mov     r2, rINST, lsr #8           @ r2<- A+
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15                 @ r2<- A
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[A]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IGET_WIDE_VOLATILE */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IGET_WIDE_VOLATILE_finish:
    cmp     r9, #0                      @ check object for null
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    beq     common_errNullObject        @ object was null
    .if     1
    add     r0, r9, r3                  @ r0<- address of field
    bl      dvmQuasiAtomicRead64        @ r0/r1<- contents of field
    .else
    ldrd    r0, [r9, r3]                @ r0/r1<- obj.field (64-bit align ok)
    .endif
    mov     r2, rINST, lsr #8           @ r2<- A+
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15                 @ r2<- A
    add     r3, rFP, r2, lsl #2         @ r3<- &fp[A]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r3, {r0-r1}                 @ fp[A]<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_IPUT_WIDE_VOLATILE */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IPUT_WIDE_VOLATILE_finish:
    mov     r2, rINST, lsr #8           @ r2<- A+
    cmp     r9, #0                      @ check object for null
    and     r2, r2, #15                 @ r2<- A
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    add     r2, rFP, r2, lsl #2         @ r3<- &fp[A]
    beq     common_errNullObject        @ object was null
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldmia   r2, {r0-r1}                 @ r0/r1<- fp[A]
    GET_INST_OPCODE(r10)                @ extract opcode from rINST
    .if     1
    add     r2, r9, r3                  @ r2<- target address
    bl      dvmQuasiAtomicSwap64Sync    @ stores r0/r1 into addr r2
    .else
    strd    r0, [r9, r3]                @ obj.field (64 bits, aligned)<- r0/r1
    .endif
    GOTO_OPCODE(r10)                    @ jump to next instruction

/* continuation for OP_SGET_WIDE_VOLATILE */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     *
     * Returns StaticField pointer in r0.
     */
.LOP_SGET_WIDE_VOLATILE_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r1<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SGET_WIDE_VOLATILE_finish          @ resume

/* continuation for OP_SPUT_WIDE_VOLATILE */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r9:  &fp[AA]
     *  r10: dvmDex->pResFields
     *
     * Returns StaticField pointer in r2.
     */
.LOP_SPUT_WIDE_VOLATILE_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    mov     r2, r0                      @ copy to r2
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SPUT_WIDE_VOLATILE_finish          @ resume

/* continuation for OP_EXECUTE_INLINE */

    /*
     * Extract args, call function.
     *  r0 = #of args (0-4)
     *  r10 = call index
     *  lr = return addr, above  [DO NOT bl out of here w/o preserving LR]
     *
     * Other ideas:
     * - Use a jump table from the main piece to jump directly into the
     *   AND/LDR pairs.  Costs a data load, saves a branch.
     * - Have five separate pieces that do the loading, so we can work the
     *   interleave a little better.  Increases code size.
     */
.LOP_EXECUTE_INLINE_continue:
    rsb     r0, r0, #4                  @ r0<- 4-r0
    FETCH(rINST, 2)                     @ rINST<- FEDC
    add     pc, pc, r0, lsl #3          @ computed goto, 2 instrs each
    bl      common_abort                @ (skipped due to ARM prefetch)
4:  and     ip, rINST, #0xf000          @ isolate F
    ldr     r3, [rFP, ip, lsr #10]      @ r3<- vF (shift right 12, left 2)
3:  and     ip, rINST, #0x0f00          @ isolate E
    ldr     r2, [rFP, ip, lsr #6]       @ r2<- vE
2:  and     ip, rINST, #0x00f0          @ isolate D
    ldr     r1, [rFP, ip, lsr #2]       @ r1<- vD
1:  and     ip, rINST, #0x000f          @ isolate C
    ldr     r0, [rFP, ip, lsl #2]       @ r0<- vC
0:
    ldr     rINST, .LOP_EXECUTE_INLINE_table    @ table of InlineOperation
5:  add     rINST, pc
    ldr     pc, [rINST, r10, lsl #4]    @ sizeof=16, "func" is first entry
    @ (not reached)

    /*
     * We're debugging or profiling.
     * r10: opIndex
     */
.LOP_EXECUTE_INLINE_debugmode:
    mov     r0, r10
    bl      dvmResolveInlineNative
    cmp     r0, #0                      @ did it resolve?
    beq     .LOP_EXECUTE_INLINE_resume          @ no, just move on
    mov     r9, r0                      @ remember method
    mov     r1, rSELF
    bl      dvmFastMethodTraceEnter     @ (method, self)
    add     r1, rSELF, #offThread_retval@ r1<- &self->retval
    sub     sp, sp, #8                  @ make room for arg, +64 bit align
    mov     r0, rINST, lsr #12          @ r0<- B
    str     r1, [sp]                    @ push &self->retval
    bl      .LOP_EXECUTE_INLINE_continue        @ make call; will return after
    mov     rINST, r0                   @ save result of inline
    add     sp, sp, #8                  @ pop stack
    mov     r0, r9                      @ r0<- method
    mov     r1, rSELF
    bl      dvmFastNativeMethodTraceExit @ (method, self)
    cmp     rINST, #0                   @ test boolean result of inline
    beq     common_exceptionThrown      @ returned false, handle exception
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction




.LOP_EXECUTE_INLINE_table:
    .word   PCREL_REF(gDvmInlineOpsTable,5b)

/* continuation for OP_EXECUTE_INLINE_RANGE */

    /*
     * Extract args, call function.
     *  r0 = #of args (0-4)
     *  r10 = call index
     *  lr = return addr, above  [DO NOT bl out of here w/o preserving LR]
     */
.LOP_EXECUTE_INLINE_RANGE_continue:
    rsb     r0, r0, #4                  @ r0<- 4-r0
    FETCH(r9, 2)                        @ r9<- CCCC
    add     pc, pc, r0, lsl #3          @ computed goto, 2 instrs each
    bl      common_abort                @ (skipped due to ARM prefetch)
4:  add     ip, r9, #3                  @ base+3
    GET_VREG(r3, ip)                    @ r3<- vBase[3]
3:  add     ip, r9, #2                  @ base+2
    GET_VREG(r2, ip)                    @ r2<- vBase[2]
2:  add     ip, r9, #1                  @ base+1
    GET_VREG(r1, ip)                    @ r1<- vBase[1]
1:  add     ip, r9, #0                  @ (nop)
    GET_VREG(r0, ip)                    @ r0<- vBase[0]
0:
    ldr     r9, .LOP_EXECUTE_INLINE_RANGE_table       @ table of InlineOperation
5:  add     r9, pc
    ldr     pc, [r9, r10, lsl #4]       @ sizeof=16, "func" is first entry
    @ (not reached)


    /*
     * We're debugging or profiling.
     * r10: opIndex
     */
.LOP_EXECUTE_INLINE_RANGE_debugmode:
    mov     r0, r10
    bl      dvmResolveInlineNative
    cmp     r0, #0                      @ did it resolve?
    beq     .LOP_EXECUTE_INLINE_RANGE_resume          @ no, just move on
    mov     r9, r0                      @ remember method
    mov     r1, rSELF
    bl      dvmFastMethodTraceEnter     @ (method, self)
    add     r1, rSELF, #offThread_retval@ r1<- &self->retval
    sub     sp, sp, #8                  @ make room for arg, +64 bit align
    mov     r0, rINST, lsr #8           @ r0<- B
    mov     rINST, r9                   @ rINST<- method
    str     r1, [sp]                    @ push &self->retval
    bl      .LOP_EXECUTE_INLINE_RANGE_continue        @ make call; will return after
    mov     r9, r0                      @ save result of inline
    add     sp, sp, #8                  @ pop stack
    mov     r0, rINST                   @ r0<- method
    mov     r1, rSELF
    bl      dvmFastNativeMethodTraceExit  @ (method, self)
    cmp     r9, #0                      @ test boolean result of inline
    beq     common_exceptionThrown      @ returned false, handle exception
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction




.LOP_EXECUTE_INLINE_RANGE_table:
    .word   PCREL_REF(gDvmInlineOpsTable,5b)


/* continuation for OP_INVOKE_OBJECT_INIT_RANGE */

.LOP_INVOKE_OBJECT_INIT_RANGE_setFinal:
    EXPORT_PC()                         @ can throw
    bl      dvmSetFinalizable           @ call dvmSetFinalizable(obj)
    ldr     r0, [rSELF, #offThread_exception] @ r0<- self->exception
    cmp     r0, #0                      @ exception pending?
    bne     common_exceptionThrown      @ yes, handle it
    b       .LOP_INVOKE_OBJECT_INIT_RANGE_finish

    /*
     * A debugger is attached, so we need to go ahead and do
     * this.  For simplicity, we'll just jump directly to the
     * corresponding handler.  Note that we can't use
     * rIBASE here because it may be in single-step mode.
     * Load the primary table base directly.
     */
.LOP_INVOKE_OBJECT_INIT_RANGE_debugger:
    ldr     r1, [rSELF, #offThread_mainHandlerTable]
    mov     ip, #OP_INVOKE_DIRECT_RANGE
    GOTO_OPCODE_BASE(r1,ip)             @ execute it

/* continuation for OP_IPUT_OBJECT_VOLATILE */

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.LOP_IPUT_OBJECT_VOLATILE_finish:
    @bl      common_squeak0
    mov     r1, rINST, lsr #8           @ r1<- A+
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    and     r1, r1, #15                 @ r1<- A
    cmp     r9, #0                      @ check object for null
    GET_VREG(r0, r1)                    @ r0<- fp[A]
    ldr     r2, [rSELF, #offThread_cardTable]  @ r2<- card table base
    beq     common_errNullObject        @ object was null
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SMP_DMB_ST                        @ releasing store
    str     r0, [r9, r3]                @ obj.field (32 bits)<- r0
    SMP_DMB
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    cmp     r0, #0                      @ stored a null reference?
    strneb  r2, [r2, r9, lsr #GC_CARD_SHIFT]  @ mark card if not
    GOTO_OPCODE(ip)                     @ jump to next instruction

/* continuation for OP_SGET_OBJECT_VOLATILE */

    /*
     * Continuation if the field has not yet been resolved.
     *  r1:  BBBB field ref
     *  r10: dvmDex->pResFields
     */
.LOP_SGET_OBJECT_VOLATILE_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SGET_OBJECT_VOLATILE_finish

/* continuation for OP_SPUT_OBJECT_VOLATILE */


.LOP_SPUT_OBJECT_VOLATILE_end:
    str     r1, [r0, #offStaticField_value]  @ field<- vAA
    SMP_DMB
    cmp     r1, #0                      @ stored a null object?
    strneb  r2, [r2, r9, lsr #GC_CARD_SHIFT]  @ mark card based on obj head
    GOTO_OPCODE(ip)                     @ jump to next instruction

    /* Continuation if the field has not yet been resolved.
     * r1:  BBBB field ref
     * r10: dvmDex->pResFields
     */
.LOP_SPUT_OBJECT_VOLATILE_resolve:
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
#if defined(WITH_JIT)
    add     r10, r10, r1, lsl #2        @ r10<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                         @ resolve() could throw, so export now
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveStaticField       @ r0<- resolved StaticField ptr
    cmp     r0, #0                      @ success?
    beq     common_exceptionThrown      @ no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    bl      common_verifyField
#endif
    b       .LOP_SPUT_OBJECT_VOLATILE_finish          @ resume


    .size   dvmAsmSisterStart, .-dvmAsmSisterStart
    .global dvmAsmSisterEnd
dvmAsmSisterEnd:


    .global dvmAsmAltInstructionStart
    .type   dvmAsmAltInstructionStart, %function
    .text

dvmAsmAltInstructionStart = .L_ALT_OP_NOP
/* ------------------------------ */
    .balign 64
.L_ALT_OP_NOP: /* 0x00 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (0 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE: /* 0x01 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (1 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE_FROM16: /* 0x02 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (2 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE_16: /* 0x03 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (3 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE_WIDE: /* 0x04 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (4 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE_WIDE_FROM16: /* 0x05 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (5 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE_WIDE_16: /* 0x06 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (6 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE_OBJECT: /* 0x07 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (7 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE_OBJECT_FROM16: /* 0x08 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (8 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE_OBJECT_16: /* 0x09 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (9 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE_RESULT: /* 0x0a */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (10 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE_RESULT_WIDE: /* 0x0b */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (11 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE_RESULT_OBJECT: /* 0x0c */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (12 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MOVE_EXCEPTION: /* 0x0d */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (13 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_RETURN_VOID: /* 0x0e */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (14 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_RETURN: /* 0x0f */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (15 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_RETURN_WIDE: /* 0x10 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (16 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_RETURN_OBJECT: /* 0x11 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (17 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CONST_4: /* 0x12 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (18 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CONST_16: /* 0x13 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (19 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CONST: /* 0x14 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (20 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CONST_HIGH16: /* 0x15 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (21 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CONST_WIDE_16: /* 0x16 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (22 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CONST_WIDE_32: /* 0x17 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (23 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CONST_WIDE: /* 0x18 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (24 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CONST_WIDE_HIGH16: /* 0x19 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (25 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CONST_STRING: /* 0x1a */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (26 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CONST_STRING_JUMBO: /* 0x1b */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (27 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CONST_CLASS: /* 0x1c */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (28 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MONITOR_ENTER: /* 0x1d */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (29 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MONITOR_EXIT: /* 0x1e */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (30 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CHECK_CAST: /* 0x1f */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (31 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INSTANCE_OF: /* 0x20 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (32 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_ARRAY_LENGTH: /* 0x21 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (33 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_NEW_INSTANCE: /* 0x22 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (34 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_NEW_ARRAY: /* 0x23 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (35 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_FILLED_NEW_ARRAY: /* 0x24 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (36 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_FILLED_NEW_ARRAY_RANGE: /* 0x25 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (37 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_FILL_ARRAY_DATA: /* 0x26 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (38 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_THROW: /* 0x27 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (39 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_GOTO: /* 0x28 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (40 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_GOTO_16: /* 0x29 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (41 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_GOTO_32: /* 0x2a */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (42 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_PACKED_SWITCH: /* 0x2b */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (43 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SPARSE_SWITCH: /* 0x2c */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (44 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CMPL_FLOAT: /* 0x2d */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (45 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CMPG_FLOAT: /* 0x2e */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (46 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CMPL_DOUBLE: /* 0x2f */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (47 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CMPG_DOUBLE: /* 0x30 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (48 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_CMP_LONG: /* 0x31 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (49 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IF_EQ: /* 0x32 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (50 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IF_NE: /* 0x33 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (51 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IF_LT: /* 0x34 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (52 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IF_GE: /* 0x35 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (53 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IF_GT: /* 0x36 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (54 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IF_LE: /* 0x37 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (55 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IF_EQZ: /* 0x38 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (56 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IF_NEZ: /* 0x39 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (57 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IF_LTZ: /* 0x3a */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (58 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IF_GEZ: /* 0x3b */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (59 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IF_GTZ: /* 0x3c */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (60 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IF_LEZ: /* 0x3d */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (61 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_UNUSED_3E: /* 0x3e */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (62 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_UNUSED_3F: /* 0x3f */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (63 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_UNUSED_40: /* 0x40 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (64 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_UNUSED_41: /* 0x41 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (65 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_UNUSED_42: /* 0x42 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (66 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_UNUSED_43: /* 0x43 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (67 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AGET: /* 0x44 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (68 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AGET_WIDE: /* 0x45 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (69 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AGET_OBJECT: /* 0x46 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (70 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AGET_BOOLEAN: /* 0x47 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (71 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AGET_BYTE: /* 0x48 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (72 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AGET_CHAR: /* 0x49 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (73 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AGET_SHORT: /* 0x4a */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (74 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_APUT: /* 0x4b */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (75 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_APUT_WIDE: /* 0x4c */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (76 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_APUT_OBJECT: /* 0x4d */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (77 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_APUT_BOOLEAN: /* 0x4e */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (78 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_APUT_BYTE: /* 0x4f */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (79 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_APUT_CHAR: /* 0x50 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (80 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_APUT_SHORT: /* 0x51 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (81 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET: /* 0x52 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (82 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET_WIDE: /* 0x53 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (83 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET_OBJECT: /* 0x54 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (84 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET_BOOLEAN: /* 0x55 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (85 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET_BYTE: /* 0x56 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (86 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET_CHAR: /* 0x57 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (87 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET_SHORT: /* 0x58 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (88 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT: /* 0x59 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (89 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT_WIDE: /* 0x5a */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (90 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT_OBJECT: /* 0x5b */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (91 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT_BOOLEAN: /* 0x5c */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (92 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT_BYTE: /* 0x5d */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (93 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT_CHAR: /* 0x5e */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (94 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT_SHORT: /* 0x5f */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (95 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SGET: /* 0x60 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (96 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SGET_WIDE: /* 0x61 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (97 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SGET_OBJECT: /* 0x62 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (98 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SGET_BOOLEAN: /* 0x63 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (99 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SGET_BYTE: /* 0x64 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (100 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SGET_CHAR: /* 0x65 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (101 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SGET_SHORT: /* 0x66 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (102 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SPUT: /* 0x67 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (103 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SPUT_WIDE: /* 0x68 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (104 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SPUT_OBJECT: /* 0x69 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (105 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SPUT_BOOLEAN: /* 0x6a */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (106 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SPUT_BYTE: /* 0x6b */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (107 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SPUT_CHAR: /* 0x6c */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (108 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SPUT_SHORT: /* 0x6d */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (109 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_VIRTUAL: /* 0x6e */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (110 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_SUPER: /* 0x6f */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (111 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_DIRECT: /* 0x70 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (112 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_STATIC: /* 0x71 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (113 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_INTERFACE: /* 0x72 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (114 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_UNUSED_73: /* 0x73 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (115 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_VIRTUAL_RANGE: /* 0x74 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (116 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_SUPER_RANGE: /* 0x75 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (117 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_DIRECT_RANGE: /* 0x76 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (118 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_STATIC_RANGE: /* 0x77 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (119 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_INTERFACE_RANGE: /* 0x78 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (120 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_UNUSED_79: /* 0x79 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (121 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_UNUSED_7A: /* 0x7a */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (122 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_NEG_INT: /* 0x7b */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (123 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_NOT_INT: /* 0x7c */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (124 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_NEG_LONG: /* 0x7d */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (125 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_NOT_LONG: /* 0x7e */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (126 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_NEG_FLOAT: /* 0x7f */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (127 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_NEG_DOUBLE: /* 0x80 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (128 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INT_TO_LONG: /* 0x81 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (129 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INT_TO_FLOAT: /* 0x82 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (130 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INT_TO_DOUBLE: /* 0x83 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (131 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_LONG_TO_INT: /* 0x84 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (132 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_LONG_TO_FLOAT: /* 0x85 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (133 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_LONG_TO_DOUBLE: /* 0x86 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (134 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_FLOAT_TO_INT: /* 0x87 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (135 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_FLOAT_TO_LONG: /* 0x88 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (136 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_FLOAT_TO_DOUBLE: /* 0x89 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (137 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DOUBLE_TO_INT: /* 0x8a */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (138 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DOUBLE_TO_LONG: /* 0x8b */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (139 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DOUBLE_TO_FLOAT: /* 0x8c */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (140 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INT_TO_BYTE: /* 0x8d */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (141 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INT_TO_CHAR: /* 0x8e */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (142 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INT_TO_SHORT: /* 0x8f */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (143 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_ADD_INT: /* 0x90 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (144 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SUB_INT: /* 0x91 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (145 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MUL_INT: /* 0x92 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (146 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DIV_INT: /* 0x93 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (147 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_REM_INT: /* 0x94 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (148 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AND_INT: /* 0x95 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (149 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_OR_INT: /* 0x96 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (150 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_XOR_INT: /* 0x97 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (151 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SHL_INT: /* 0x98 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (152 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SHR_INT: /* 0x99 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (153 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_USHR_INT: /* 0x9a */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (154 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_ADD_LONG: /* 0x9b */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (155 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SUB_LONG: /* 0x9c */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (156 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MUL_LONG: /* 0x9d */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (157 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DIV_LONG: /* 0x9e */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (158 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_REM_LONG: /* 0x9f */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (159 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AND_LONG: /* 0xa0 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (160 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_OR_LONG: /* 0xa1 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (161 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_XOR_LONG: /* 0xa2 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (162 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SHL_LONG: /* 0xa3 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (163 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SHR_LONG: /* 0xa4 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (164 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_USHR_LONG: /* 0xa5 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (165 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_ADD_FLOAT: /* 0xa6 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (166 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SUB_FLOAT: /* 0xa7 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (167 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MUL_FLOAT: /* 0xa8 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (168 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DIV_FLOAT: /* 0xa9 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (169 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_REM_FLOAT: /* 0xaa */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (170 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_ADD_DOUBLE: /* 0xab */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (171 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SUB_DOUBLE: /* 0xac */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (172 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MUL_DOUBLE: /* 0xad */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (173 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DIV_DOUBLE: /* 0xae */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (174 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_REM_DOUBLE: /* 0xaf */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (175 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_ADD_INT_2ADDR: /* 0xb0 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (176 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SUB_INT_2ADDR: /* 0xb1 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (177 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MUL_INT_2ADDR: /* 0xb2 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (178 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DIV_INT_2ADDR: /* 0xb3 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (179 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_REM_INT_2ADDR: /* 0xb4 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (180 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AND_INT_2ADDR: /* 0xb5 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (181 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_OR_INT_2ADDR: /* 0xb6 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (182 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_XOR_INT_2ADDR: /* 0xb7 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (183 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SHL_INT_2ADDR: /* 0xb8 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (184 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SHR_INT_2ADDR: /* 0xb9 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (185 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_USHR_INT_2ADDR: /* 0xba */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (186 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_ADD_LONG_2ADDR: /* 0xbb */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (187 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SUB_LONG_2ADDR: /* 0xbc */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (188 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MUL_LONG_2ADDR: /* 0xbd */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (189 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DIV_LONG_2ADDR: /* 0xbe */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (190 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_REM_LONG_2ADDR: /* 0xbf */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (191 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AND_LONG_2ADDR: /* 0xc0 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (192 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_OR_LONG_2ADDR: /* 0xc1 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (193 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_XOR_LONG_2ADDR: /* 0xc2 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (194 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SHL_LONG_2ADDR: /* 0xc3 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (195 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SHR_LONG_2ADDR: /* 0xc4 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (196 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_USHR_LONG_2ADDR: /* 0xc5 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (197 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_ADD_FLOAT_2ADDR: /* 0xc6 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (198 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SUB_FLOAT_2ADDR: /* 0xc7 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (199 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MUL_FLOAT_2ADDR: /* 0xc8 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (200 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DIV_FLOAT_2ADDR: /* 0xc9 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (201 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_REM_FLOAT_2ADDR: /* 0xca */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (202 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_ADD_DOUBLE_2ADDR: /* 0xcb */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (203 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SUB_DOUBLE_2ADDR: /* 0xcc */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (204 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MUL_DOUBLE_2ADDR: /* 0xcd */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (205 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DIV_DOUBLE_2ADDR: /* 0xce */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (206 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_REM_DOUBLE_2ADDR: /* 0xcf */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (207 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_ADD_INT_LIT16: /* 0xd0 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (208 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_RSUB_INT: /* 0xd1 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (209 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MUL_INT_LIT16: /* 0xd2 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (210 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DIV_INT_LIT16: /* 0xd3 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (211 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_REM_INT_LIT16: /* 0xd4 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (212 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AND_INT_LIT16: /* 0xd5 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (213 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_OR_INT_LIT16: /* 0xd6 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (214 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_XOR_INT_LIT16: /* 0xd7 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (215 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_ADD_INT_LIT8: /* 0xd8 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (216 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_RSUB_INT_LIT8: /* 0xd9 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (217 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_MUL_INT_LIT8: /* 0xda */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (218 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_DIV_INT_LIT8: /* 0xdb */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (219 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_REM_INT_LIT8: /* 0xdc */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (220 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_AND_INT_LIT8: /* 0xdd */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (221 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_OR_INT_LIT8: /* 0xde */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (222 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_XOR_INT_LIT8: /* 0xdf */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (223 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SHL_INT_LIT8: /* 0xe0 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (224 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SHR_INT_LIT8: /* 0xe1 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (225 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_USHR_INT_LIT8: /* 0xe2 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (226 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET_VOLATILE: /* 0xe3 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (227 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT_VOLATILE: /* 0xe4 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (228 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SGET_VOLATILE: /* 0xe5 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (229 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SPUT_VOLATILE: /* 0xe6 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (230 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET_OBJECT_VOLATILE: /* 0xe7 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (231 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET_WIDE_VOLATILE: /* 0xe8 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (232 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT_WIDE_VOLATILE: /* 0xe9 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (233 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SGET_WIDE_VOLATILE: /* 0xea */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (234 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SPUT_WIDE_VOLATILE: /* 0xeb */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (235 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_BREAKPOINT: /* 0xec */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (236 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_THROW_VERIFICATION_ERROR: /* 0xed */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (237 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_EXECUTE_INLINE: /* 0xee */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (238 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_EXECUTE_INLINE_RANGE: /* 0xef */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (239 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_OBJECT_INIT_RANGE: /* 0xf0 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (240 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_RETURN_VOID_BARRIER: /* 0xf1 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (241 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET_QUICK: /* 0xf2 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (242 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET_WIDE_QUICK: /* 0xf3 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (243 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IGET_OBJECT_QUICK: /* 0xf4 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (244 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT_QUICK: /* 0xf5 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (245 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT_WIDE_QUICK: /* 0xf6 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (246 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT_OBJECT_QUICK: /* 0xf7 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (247 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_VIRTUAL_QUICK: /* 0xf8 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (248 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_VIRTUAL_QUICK_RANGE: /* 0xf9 */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (249 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_SUPER_QUICK: /* 0xfa */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (250 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_INVOKE_SUPER_QUICK_RANGE: /* 0xfb */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (251 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_IPUT_OBJECT_VOLATILE: /* 0xfc */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (252 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SGET_OBJECT_VOLATILE: /* 0xfd */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (253 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_SPUT_OBJECT_VOLATILE: /* 0xfe */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (254 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

/* ------------------------------ */
    .balign 64
.L_ALT_OP_UNUSED_FF: /* 0xff */
/* File: armv5te/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (255 * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call

    .balign 64
    .size   dvmAsmAltInstructionStart, .-dvmAsmAltInstructionStart
    .global dvmAsmAltInstructionEnd
dvmAsmAltInstructionEnd:
/* File: armv5te/footer.S */
/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .text
    .align  2

#if defined(WITH_JIT)

#if defined(WITH_SELF_VERIFICATION)
/*
 * "longjmp" to a translation after single-stepping.  Before returning
 * to translation, must save state for self-verification.
 */
    .global dvmJitResumeTranslation              @ (Thread* self, u4* dFP)
dvmJitResumeTranslation:
    mov    rSELF, r0                             @ restore self
    mov    rPC, r1                               @ restore Dalvik pc
    mov    rFP, r2                               @ restore Dalvik fp
    ldr    r10, [rSELF,#offThread_jitResumeNPC]  @ resume address
    mov    r2, #0
    str    r2, [rSELF,#offThread_jitResumeNPC]   @ reset resume address
    ldr    sp, [rSELF,#offThread_jitResumeNSP]   @ cut back native stack
    b      jitSVShadowRunStart                   @ resume as if cache hit
                                                 @ expects resume addr in r10

    .global dvmJitToInterpPunt
dvmJitToInterpPunt:
    mov    r2,#kSVSPunt                 @ r2<- interpreter entry point
    mov    r3, #0
    str    r3, [rSELF, #offThread_inJitCodeCache] @ Back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return

    .global dvmJitToInterpSingleStep
dvmJitToInterpSingleStep:
    mov    rPC, r0              @ set up dalvik pc
    EXPORT_PC()
    str    lr, [rSELF,#offThread_jitResumeNPC]
    str    sp, [rSELF,#offThread_jitResumeNSP]
    str    r1, [rSELF,#offThread_jitResumeDPC]
    mov    r2,#kSVSSingleStep           @ r2<- interpreter entry point
    b      jitSVShadowRunEnd            @ doesn't return


    .global dvmJitToInterpNoChainNoProfile
dvmJitToInterpNoChainNoProfile:
    mov    r0,rPC                       @ pass our target PC
    mov    r2,#kSVSNoProfile            @ r2<- interpreter entry point
    mov    r3, #0                       @ 0 means !inJitCodeCache
    str    r3, [rSELF, #offThread_inJitCodeCache] @ back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return

    .global dvmJitToInterpTraceSelectNoChain
dvmJitToInterpTraceSelectNoChain:
    mov    r0,rPC                       @ pass our target PC
    mov    r2,#kSVSTraceSelect          @ r2<- interpreter entry point
    mov    r3, #0                       @ 0 means !inJitCodeCache
    str    r3, [rSELF, #offThread_inJitCodeCache] @ Back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return

    .global dvmJitToInterpTraceSelect
dvmJitToInterpTraceSelect:
    ldr    r0,[lr, #-1]                 @ pass our target PC
    mov    r2,#kSVSTraceSelect          @ r2<- interpreter entry point
    mov    r3, #0                       @ 0 means !inJitCodeCache
    str    r3, [rSELF, #offThread_inJitCodeCache] @ Back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return

    .global dvmJitToInterpBackwardBranch
dvmJitToInterpBackwardBranch:
    ldr    r0,[lr, #-1]                 @ pass our target PC
    mov    r2,#kSVSBackwardBranch       @ r2<- interpreter entry point
    mov    r3, #0                       @ 0 means !inJitCodeCache
    str    r3, [rSELF, #offThread_inJitCodeCache] @ Back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return

    .global dvmJitToInterpNormal
dvmJitToInterpNormal:
    ldr    r0,[lr, #-1]                 @ pass our target PC
    mov    r2,#kSVSNormal               @ r2<- interpreter entry point
    mov    r3, #0                       @ 0 means !inJitCodeCache
    str    r3, [rSELF, #offThread_inJitCodeCache] @ Back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return

    .global dvmJitToInterpNoChain
dvmJitToInterpNoChain:
    mov    r0,rPC                       @ pass our target PC
    mov    r2,#kSVSNoChain              @ r2<- interpreter entry point
    mov    r3, #0                       @ 0 means !inJitCodeCache
    str    r3, [rSELF, #offThread_inJitCodeCache] @ Back to the interp land
    b      jitSVShadowRunEnd            @ doesn't return
#else

/*
 * "longjmp" to a translation after single-stepping.
 */
    .global dvmJitResumeTranslation              @ (Thread* self, u4* dFP)
dvmJitResumeTranslation:
    mov    rSELF, r0                             @ restore self
    mov    rPC, r1                               @ restore Dalvik pc
    mov    rFP, r2                               @ restore Dalvik fp
    ldr    r0, [rSELF,#offThread_jitResumeNPC]
    mov    r2, #0
    str    r2, [rSELF,#offThread_jitResumeNPC]   @ reset resume address
    ldr    sp, [rSELF,#offThread_jitResumeNSP]   @ cut back native stack
    bx     r0                                    @ resume translation

/*
 * Return from the translation cache to the interpreter when the compiler is
 * having issues translating/executing a Dalvik instruction. We have to skip
 * the code cache lookup otherwise it is possible to indefinitely bouce
 * between the interpreter and the code cache if the instruction that fails
 * to be compiled happens to be at a trace start.
 */
    .global dvmJitToInterpPunt
dvmJitToInterpPunt:
    mov    rPC, r0
#if defined(WITH_JIT_TUNING)
    mov    r0,lr
    bl     dvmBumpPunt;
#endif
    EXPORT_PC()
    mov    r0, #0
    str    r0, [rSELF, #offThread_inJitCodeCache] @ Back to the interp land
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    FETCH_INST()
    GET_INST_OPCODE(ip)
    GOTO_OPCODE(ip)

/*
 * Return to the interpreter to handle a single instruction.
 * We'll use the normal single-stepping mechanism via interpBreak,
 * but also save the native pc of the resume point in the translation
 * and the native sp so that we can later do the equivalent of a
 * longjmp() to resume.
 * On entry:
 *    dPC <= Dalvik PC of instrucion to interpret
 *    lr <= resume point in translation
 *    r1 <= Dalvik PC of next instruction
 */
    .global dvmJitToInterpSingleStep
dvmJitToInterpSingleStep:
    mov    rPC, r0              @ set up dalvik pc
    EXPORT_PC()
    str    lr, [rSELF,#offThread_jitResumeNPC]
    str    sp, [rSELF,#offThread_jitResumeNSP]
    str    r1, [rSELF,#offThread_jitResumeDPC]
    mov    r1, #1
    str    r1, [rSELF,#offThread_singleStepCount]  @ just step once
    mov    r0, rSELF
    mov    r1, #kSubModeCountedStep
    bl     dvmEnableSubMode     @ (self, newMode)
    ldr    rIBASE, [rSELF,#offThread_curHandlerTable]
    FETCH_INST()
    GET_INST_OPCODE(ip)
    GOTO_OPCODE(ip)

/*
 * Return from the translation cache and immediately request
 * a translation for the exit target.  Commonly used for callees.
 */
    .global dvmJitToInterpTraceSelectNoChain
dvmJitToInterpTraceSelectNoChain:
#if defined(WITH_JIT_TUNING)
    bl     dvmBumpNoChain
#endif
    mov    r0,rPC
    mov    r1,rSELF
    bl     dvmJitGetTraceAddrThread @ (pc, self)
    str    r0, [rSELF, #offThread_inJitCodeCache] @ set the inJitCodeCache flag
    mov    r1, rPC                  @ arg1 of translation may need this
    mov    lr, #0                   @  in case target is HANDLER_INTERPRET
    cmp    r0,#0                    @ !0 means translation exists
    bxne   r0                       @ continue native execution if so
    b      2f                       @ branch over to use the interpreter

/*
 * Return from the translation cache and immediately request
 * a translation for the exit target.  Commonly used following
 * invokes.
 */
    .global dvmJitToInterpTraceSelect
dvmJitToInterpTraceSelect:
    ldr    rPC,[lr, #-1]           @ get our target PC
    add    rINST,lr,#-5            @ save start of chain branch
    add    rINST, #-4              @  .. which is 9 bytes back
    mov    r0,rPC
    mov    r1,rSELF
    bl     dvmJitGetTraceAddrThread @ (pc, self)
    str    r0, [rSELF, #offThread_inJitCodeCache] @ set the inJitCodeCache flag
    cmp    r0,#0
    beq    2f
    mov    r1,rINST
    bl     dvmJitChain              @ r0<- dvmJitChain(codeAddr,chainAddr)
    mov    r1, rPC                  @ arg1 of translation may need this
    mov    lr, #0                   @ in case target is HANDLER_INTERPRET
    cmp    r0,#0                    @ successful chain?
    bxne   r0                       @ continue native execution
    b      toInterpreter            @ didn't chain - resume with interpreter

/* No translation, so request one if profiling isn't disabled*/
2:
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    ldr    r0, [rSELF, #offThread_pJitProfTable]
    FETCH_INST()
    cmp    r0, #0
    movne  r2,#kJitTSelectRequestHot   @ ask for trace selection
    bne    common_selectTrace
    GET_INST_OPCODE(ip)
    GOTO_OPCODE(ip)

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
    ldr    rPC,[lr, #-1]           @ get our target PC
    add    rINST,lr,#-5            @ save start of chain branch
    add    rINST,#-4               @ .. which is 9 bytes back
#if defined(WITH_JIT_TUNING)
    bl     dvmBumpNormal
#endif
    mov    r0,rPC
    mov    r1,rSELF
    bl     dvmJitGetTraceAddrThread @ (pc, self)
    str    r0, [rSELF, #offThread_inJitCodeCache] @ set the inJitCodeCache flag
    cmp    r0,#0
    beq    toInterpreter            @ go if not, otherwise do chain
    mov    r1,rINST
    bl     dvmJitChain              @ r0<- dvmJitChain(codeAddr,chainAddr)
    mov    r1, rPC                  @ arg1 of translation may need this
    mov    lr, #0                   @  in case target is HANDLER_INTERPRET
    cmp    r0,#0                    @ successful chain?
    bxne   r0                       @ continue native execution
    b      toInterpreter            @ didn't chain - resume with interpreter

/*
 * Return from the translation cache to the interpreter to do method invocation.
 * Check if translation exists for the callee, but don't chain to it.
 */
    .global dvmJitToInterpNoChainNoProfile
dvmJitToInterpNoChainNoProfile:
#if defined(WITH_JIT_TUNING)
    bl     dvmBumpNoChain
#endif
    mov    r0,rPC
    mov    r1,rSELF
    bl     dvmJitGetTraceAddrThread @ (pc, self)
    str    r0, [rSELF, #offThread_inJitCodeCache] @ set the inJitCodeCache flag
    mov    r1, rPC                  @ arg1 of translation may need this
    mov    lr, #0                   @  in case target is HANDLER_INTERPRET
    cmp    r0,#0
    bxne   r0                       @ continue native execution if so
    EXPORT_PC()
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    FETCH_INST()
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/*
 * Return from the translation cache to the interpreter to do method invocation.
 * Check if translation exists for the callee, but don't chain to it.
 */
    .global dvmJitToInterpNoChain
dvmJitToInterpNoChain:
#if defined(WITH_JIT_TUNING)
    bl     dvmBumpNoChain
#endif
    mov    r0,rPC
    mov    r1,rSELF
    bl     dvmJitGetTraceAddrThread @ (pc, self)
    str    r0, [rSELF, #offThread_inJitCodeCache] @ set the inJitCodeCache flag
    mov    r1, rPC                  @ arg1 of translation may need this
    mov    lr, #0                   @  in case target is HANDLER_INTERPRET
    cmp    r0,#0
    bxne   r0                       @ continue native execution if so
#endif

/*
 * No translation, restore interpreter regs and start interpreting.
 * rSELF & rFP were preserved in the translated code, and rPC has
 * already been restored by the time we get here.  We'll need to set
 * up rIBASE & rINST, and load the address of the JitTable into r0.
 */
toInterpreter:
    EXPORT_PC()
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    FETCH_INST()
    ldr    r0, [rSELF, #offThread_pJitProfTable]
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    @ NOTE: intended fallthrough

/*
 * Similar to common_updateProfile, but tests for null pJitProfTable
 * r0 holds pJifProfTAble, rINST is loaded, rPC is current and
 * rIBASE has been recently refreshed.
 */
common_testUpdateProfile:
    cmp     r0, #0               @ JIT switched off?
    beq     4f                   @ return to interp if so

/*
 * Common code to update potential trace start counter, and initiate
 * a trace-build if appropriate.
 * On entry here:
 *    r0    <= pJitProfTable (verified non-NULL)
 *    rPC   <= Dalvik PC
 *    rINST <= next instruction
 */
common_updateProfile:
    eor     r3,rPC,rPC,lsr #12 @ cheap, but fast hash function
    lsl     r3,r3,#(32 - JIT_PROF_SIZE_LOG_2)          @ shift out excess bits
    ldrb    r1,[r0,r3,lsr #(32 - JIT_PROF_SIZE_LOG_2)] @ get counter
    GET_INST_OPCODE(ip)
    subs    r1,r1,#1           @ decrement counter
    strb    r1,[r0,r3,lsr #(32 - JIT_PROF_SIZE_LOG_2)] @ and store it
    GOTO_OPCODE_IFNE(ip)       @ if not threshold, fallthrough otherwise */

    /* Looks good, reset the counter */
    ldr     r1, [rSELF, #offThread_jitThreshold]
    strb    r1,[r0,r3,lsr #(32 - JIT_PROF_SIZE_LOG_2)] @ reset counter
    EXPORT_PC()
    mov     r0,rPC
    mov     r1,rSELF
    bl      dvmJitGetTraceAddrThread    @ (pc, self)
    str     r0, [rSELF, #offThread_inJitCodeCache] @ set the inJitCodeCache flag
    mov     r1, rPC                     @ arg1 of translation may need this
    mov     lr, #0                      @  in case target is HANDLER_INTERPRET
    cmp     r0,#0
#if !defined(WITH_SELF_VERIFICATION)
    bxne    r0                          @ jump to the translation
    mov     r2,#kJitTSelectRequest      @ ask for trace selection
    @ fall-through to common_selectTrace
#else
    moveq   r2,#kJitTSelectRequest      @ ask for trace selection
    beq     common_selectTrace
    /*
     * At this point, we have a target translation.  However, if
     * that translation is actually the interpret-only pseudo-translation
     * we want to treat it the same as no translation.
     */
    mov     r10, r0                     @ save target
    bl      dvmCompilerGetInterpretTemplate
    cmp     r0, r10                     @ special case?
    bne     jitSVShadowRunStart         @ set up self verification shadow space
    @ Need to clear the inJitCodeCache flag
    mov    r3, #0                       @ 0 means not in the JIT code cache
    str    r3, [rSELF, #offThread_inJitCodeCache] @ back to the interp land
    GET_INST_OPCODE(ip)
    GOTO_OPCODE(ip)
    /* no return */
#endif

/*
 * On entry:
 *  r2 is jit state.
 */
common_selectTrace:
    ldrh    r0,[rSELF,#offThread_subMode]
    ands    r0, #(kSubModeJitTraceBuild | kSubModeJitSV)
    bne     3f                         @ already doing JIT work, continue
    str     r2,[rSELF,#offThread_jitState]
    mov     r0, rSELF
/*
 * Call out to validate trace-building request.  If successful,
 * rIBASE will be swapped to to send us into single-stepping trace
 * building mode, so we need to refresh before we continue.
 */
    EXPORT_PC()
    SAVE_PC_FP_TO_SELF()                 @ copy of pc/fp to Thread
    bl      dvmJitCheckTraceRequest
3:
    FETCH_INST()
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
4:
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)
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
    mov     r0,rPC                      @ r0<- program counter
    mov     r1,rFP                      @ r1<- frame pointer
    mov     r2,rSELF                    @ r2<- self (Thread) pointer
    mov     r3,r10                      @ r3<- target translation
    bl      dvmSelfVerificationSaveState @ save registers to shadow space
    ldr     rFP,[r0,#offShadowSpace_shadowFP] @ rFP<- fp in shadow space
    bx      r10                         @ jump to the translation

/*
 * Restore PC, registers, and interpreter state to original values
 * before jumping back to the interpreter.
 * On entry:
 *   r0:  dPC
 *   r2:  self verification state
 */
jitSVShadowRunEnd:
    mov    r1,rFP                        @ pass ending fp
    mov    r3,rSELF                      @ pass self ptr for convenience
    bl     dvmSelfVerificationRestoreState @ restore pc and fp values
    LOAD_PC_FP_FROM_SELF()               @ restore pc, fp
    ldr    r1,[r0,#offShadowSpace_svState] @ get self verification state
    cmp    r1,#0                         @ check for punt condition
    beq    1f
    @ Set up SV single-stepping
    mov    r0, rSELF
    mov    r1, #kSubModeJitSV
    bl     dvmEnableSubMode              @ (self, subMode)
    mov    r2,#kJitSelfVerification      @ ask for self verification
    str    r2,[rSELF,#offThread_jitState]
    @ intentional fallthrough
1:                                       @ exit to interpreter without check
    EXPORT_PC()
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    FETCH_INST()
    GET_INST_OPCODE(ip)
    GOTO_OPCODE(ip)
#endif

/*
 * The equivalent of "goto bail", this calls through the "bail handler".
 * It will end this interpreter activation, and return to the caller
 * of dvmMterpStdRun.
 *
 * State registers will be saved to the "thread" area before bailing
 * debugging purposes
 */
common_gotoBail:
    SAVE_PC_FP_TO_SELF()                @ export state to "thread"
    mov     r0, rSELF                   @ r0<- self ptr
    b       dvmMterpStdBail             @ call(self, changeInterp)

/*
 * The JIT's invoke method needs to remember the callsite class and
 * target pair.  Save them here so that they are available to
 * dvmCheckJit following the interpretation of this invoke.
 */
#if defined(WITH_JIT)
save_callsiteinfo:
    cmp     r9, #0
    ldrne   r9, [r9, #offObject_clazz]
    str     r0, [rSELF, #offThread_methodToCall]
    str     r9, [rSELF, #offThread_callsiteClass]
    bx      lr
#endif

/*
 * Common code for method invocation with range.
 *
 * On entry:
 *  r0 is "Method* methodToCall", r9 is "this"
 */
common_invokeMethodRange:
.LinvokeNewRange:
#if defined(WITH_JIT)
    ldrh    r1, [rSELF, #offThread_subMode]
    ands    r1, #kSubModeJitTraceBuild
    blne    save_callsiteinfo
#endif
    @ prepare to copy args to "outs" area of current frame
    movs    r2, rINST, lsr #8           @ r2<- AA (arg count) -- test for zero
    SAVEAREA_FROM_FP(r10, rFP)          @ r10<- stack save area
    beq     .LinvokeArgsDone            @ if no args, skip the rest
    FETCH(r1, 2)                        @ r1<- CCCC

.LinvokeRangeArgs:
    @ r0=methodToCall, r1=CCCC, r2=count, r10=outs
    @ (very few methods have > 10 args; could unroll for common cases)
    add     r3, rFP, r1, lsl #2         @ r3<- &fp[CCCC]
    sub     r10, r10, r2, lsl #2        @ r10<- "outs" area, for call args
1:  ldr     r1, [r3], #4                @ val = *fp++
    subs    r2, r2, #1                  @ count--
    str     r1, [r10], #4               @ *outs++ = val
    bne     1b                          @ ...while count != 0
    b       .LinvokeArgsDone

/*
 * Common code for method invocation without range.
 *
 * On entry:
 *  r0 is "Method* methodToCall", r9 is "this"
 */
common_invokeMethodNoRange:
.LinvokeNewNoRange:
#if defined(WITH_JIT)
    ldrh    r1, [rSELF, #offThread_subMode]
    ands    r1, #kSubModeJitTraceBuild
    blne    save_callsiteinfo
#endif
    @ prepare to copy args to "outs" area of current frame
    movs    r2, rINST, lsr #12          @ r2<- B (arg count) -- test for zero
    SAVEAREA_FROM_FP(r10, rFP)          @ r10<- stack save area
    FETCH(r1, 2)                        @ r1<- GFED (load here to hide latency)
    beq     .LinvokeArgsDone

    @ r0=methodToCall, r1=GFED, r2=count, r10=outs
.LinvokeNonRange:
    rsb     r2, r2, #5                  @ r2<- 5-r2
    add     pc, pc, r2, lsl #4          @ computed goto, 4 instrs each
    bl      common_abort                @ (skipped due to ARM prefetch)
5:  and     ip, rINST, #0x0f00          @ isolate A
    ldr     r2, [rFP, ip, lsr #6]       @ r2<- vA (shift right 8, left 2)
    mov     r0, r0                      @ nop
    str     r2, [r10, #-4]!             @ *--outs = vA
4:  and     ip, r1, #0xf000             @ isolate G
    ldr     r2, [rFP, ip, lsr #10]      @ r2<- vG (shift right 12, left 2)
    mov     r0, r0                      @ nop
    str     r2, [r10, #-4]!             @ *--outs = vG
3:  and     ip, r1, #0x0f00             @ isolate F
    ldr     r2, [rFP, ip, lsr #6]       @ r2<- vF
    mov     r0, r0                      @ nop
    str     r2, [r10, #-4]!             @ *--outs = vF
2:  and     ip, r1, #0x00f0             @ isolate E
    ldr     r2, [rFP, ip, lsr #2]       @ r2<- vE
    mov     r0, r0                      @ nop
    str     r2, [r10, #-4]!             @ *--outs = vE
1:  and     ip, r1, #0x000f             @ isolate D
    ldr     r2, [rFP, ip, lsl #2]       @ r2<- vD
    mov     r0, r0                      @ nop
    str     r2, [r10, #-4]!             @ *--outs = vD
0:  @ fall through to .LinvokeArgsDone

.LinvokeArgsDone: @ r0=methodToCall
    ldrh    r9, [r0, #offMethod_registersSize]  @ r9<- methodToCall->regsSize
    ldrh    r3, [r0, #offMethod_outsSize]  @ r3<- methodToCall->outsSize
    ldr     r2, [r0, #offMethod_insns]  @ r2<- method->insns
    ldr     rINST, [r0, #offMethod_clazz]  @ rINST<- method->clazz
    @ find space for the new stack frame, check for overflow
    SAVEAREA_FROM_FP(r1, rFP)           @ r1<- stack save area
    sub     r1, r1, r9, lsl #2          @ r1<- newFp (old savearea - regsSize)
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- newSaveArea
@    bl      common_dumpRegs
    ldr     r9, [rSELF, #offThread_interpStackEnd]    @ r9<- interpStackEnd
    sub     r3, r10, r3, lsl #2         @ r3<- bottom (newsave - outsSize)
    cmp     r3, r9                      @ bottom < interpStackEnd?
    ldrh    lr, [rSELF, #offThread_subMode]
    ldr     r3, [r0, #offMethod_accessFlags] @ r3<- methodToCall->accessFlags
    blo     .LstackOverflow             @ yes, this frame will overflow stack

    @ set up newSaveArea
#ifdef EASY_GDB
    SAVEAREA_FROM_FP(ip, rFP)           @ ip<- stack save area
    str     ip, [r10, #offStackSaveArea_prevSave]
#endif
    str     rFP, [r10, #offStackSaveArea_prevFrame]
    str     rPC, [r10, #offStackSaveArea_savedPc]
#if defined(WITH_JIT)
    mov     r9, #0
    str     r9, [r10, #offStackSaveArea_returnAddr]
#endif
    str     r0, [r10, #offStackSaveArea_method]

    @ Profiling?
    cmp     lr, #0                      @ any special modes happening?
    bne     2f                          @ go if so
1:
    tst     r3, #ACC_NATIVE
    bne     .LinvokeNative

    /*
    stmfd   sp!, {r0-r3}
    bl      common_printNewline
    mov     r0, rFP
    mov     r1, #0
    bl      dvmDumpFp
    ldmfd   sp!, {r0-r3}
    stmfd   sp!, {r0-r3}
    mov     r0, r1
    mov     r1, r10
    bl      dvmDumpFp
    bl      common_printNewline
    ldmfd   sp!, {r0-r3}
    */

    ldrh    r9, [r2]                        @ r9 <- load INST from new PC
    ldr     r3, [rINST, #offClassObject_pDvmDex] @ r3<- method->clazz->pDvmDex
    mov     rPC, r2                         @ publish new rPC

    @ Update state values for the new method
    @ r0=methodToCall, r1=newFp, r3=newMethodClass, r9=newINST
    str     r0, [rSELF, #offThread_method]    @ self->method = methodToCall
    str     r3, [rSELF, #offThread_methodClassDex] @ self->methodClassDex = ...
    mov     r2, #1
    str     r2, [rSELF, #offThread_debugIsMethodEntry]
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    mov     rFP, r1                         @ fp = newFp
    GET_PREFETCHED_OPCODE(ip, r9)           @ extract prefetched opcode from r9
    mov     rINST, r9                       @ publish new rINST
    str     r1, [rSELF, #offThread_curFrame]   @ curFrame = newFp
    cmp     r0,#0
    bne     common_updateProfile
    GOTO_OPCODE(ip)                         @ jump to next instruction
#else
    mov     rFP, r1                         @ fp = newFp
    GET_PREFETCHED_OPCODE(ip, r9)           @ extract prefetched opcode from r9
    mov     rINST, r9                       @ publish new rINST
    str     r1, [rSELF, #offThread_curFrame]   @ curFrame = newFp
    GOTO_OPCODE(ip)                         @ jump to next instruction
#endif

2:
    @ Profiling - record method entry.  r0: methodToCall
    stmfd   sp!, {r0-r3}                @ preserve r0-r3
    str     rPC, [rSELF, #offThread_pc] @ update interpSave.pc
    mov     r1, r0
    mov     r0, rSELF
    bl      dvmReportInvoke             @ (self, method)
    ldmfd   sp!, {r0-r3}                @ restore r0-r3
    b       1b

.LinvokeNative:
    @ Prep for the native call
    @ r0=methodToCall, r1=newFp, r10=newSaveArea
    ldrh    lr, [rSELF, #offThread_subMode]
    ldr     r9, [rSELF, #offThread_jniLocal_topCookie]@r9<-thread->localRef->...
    str     r1, [rSELF, #offThread_curFrame]   @ curFrame = newFp
    str     r9, [r10, #offStackSaveArea_localRefCookie] @newFp->localRefCookie=top
    mov     r2, r0                      @ r2<- methodToCall
    mov     r0, r1                      @ r0<- newFp (points to args)
    add     r1, rSELF, #offThread_retval  @ r1<- &retval
    mov     r3, rSELF                   @ arg3<- self

#ifdef ASSIST_DEBUGGER
    /* insert fake function header to help gdb find the stack frame */
    b       .Lskip
    .type   dalvik_mterp, %function
dalvik_mterp:
    .fnstart
    MTERP_ENTRY1
    MTERP_ENTRY2
.Lskip:
#endif

    cmp     lr, #0                      @ any special SubModes active?
    bne     11f                         @ go handle them if so
    ldr     ip, [r2, #offMethod_nativeFunc] @ pc<- methodToCall->nativeFunc
    blx     ip
7:

    @ native return; r10=newSaveArea
    @ equivalent to dvmPopJniLocals
    ldr     r0, [r10, #offStackSaveArea_localRefCookie] @ r0<- saved top
    ldr     r1, [rSELF, #offThread_exception] @ check for exception
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = fp
    cmp     r1, #0                      @ null?
    str     r0, [rSELF, #offThread_jniLocal_topCookie] @ new top <- old top
    bne     common_exceptionThrown      @ no, handle exception

    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

11:
    @ r0=newFp, r1=&retval, r2=methodToCall, r3=self, lr=subModes
    stmfd   sp!, {r0-r3}                @ save all but subModes
    mov     r0, r2                      @ r0<- methodToCall
    mov     r1, rSELF
    mov     r2, rFP
    bl      dvmReportPreNativeInvoke    @ (methodToCall, self, fp)
    ldmfd   sp, {r0-r3}                 @ refresh.  NOTE: no sp autoincrement

    @ Call the native method
    ldr     ip, [r2, #offMethod_nativeFunc] @ pc<- methodToCall->nativeFunc
    blx     ip

    @ Restore the pre-call arguments
    ldmfd   sp!, {r0-r3}                @ r2<- methodToCall (others unneeded)

    @ Finish up any post-invoke subMode requirements
    mov     r0, r2                      @ r0<- methodToCall
    mov     r1, rSELF
    mov     r2, rFP
    bl      dvmReportPostNativeInvoke   @ (methodToCall, self, fp)
    b       7b                          @ resume

.LstackOverflow:    @ r0=methodToCall
    mov     r1, r0                      @ r1<- methodToCall
    mov     r0, rSELF                   @ r0<- self
    bl      dvmHandleStackOverflow
    b       common_exceptionThrown
#ifdef ASSIST_DEBUGGER
    .fnend
    .size   dalvik_mterp, .-dalvik_mterp
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
     .if    0
.LinvokeOld:
    sub     sp, sp, #8                  @ space for args + pad
    FETCH(ip, 2)                        @ ip<- FEDC or CCCC
    mov     r2, r0                      @ A2<- methodToCall
    mov     r0, rSELF                   @ A0<- self
    SAVE_PC_FP_TO_SELF()                @ export state to "self"
    mov     r1, r9                      @ A1<- methodCallRange
    mov     r3, rINST, lsr #8           @ A3<- AA
    str     ip, [sp, #0]                @ A4<- ip
    bl      dvmMterp_invokeMethod       @ call the C invokeMethod
    add     sp, sp, #8                  @ remove arg area
    b       common_resumeAfterGlueCall  @ continue to next instruction
    .endif



/*
 * Common code for handling a return instruction.
 *
 * This does not return.
 */
common_returnFromMethod:
.LreturnNew:
    ldrh    lr, [rSELF, #offThread_subMode]
    SAVEAREA_FROM_FP(r0, rFP)
    ldr     r9, [r0, #offStackSaveArea_savedPc] @ r9 = saveArea->savedPc
    cmp     lr, #0                      @ any special subMode handling needed?
    bne     19f
14:
    ldr     rFP, [r0, #offStackSaveArea_prevFrame] @ fp = saveArea->prevFrame
    ldr     r2, [rFP, #(offStackSaveArea_method - sizeofStackSaveArea)]
                                        @ r2<- method we're returning to
    cmp     r2, #0                      @ is this a break frame?
#if defined(WORKAROUND_CORTEX_A9_745320)
    /* Don't use conditional loads if the HW defect exists */
    beq     15f
    ldr     r10, [r2, #offMethod_clazz] @ r10<- method->clazz
15:
#else
    ldrne   r10, [r2, #offMethod_clazz] @ r10<- method->clazz
#endif
    beq     common_gotoBail             @ break frame, bail out completely

    ldr     rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
    PREFETCH_ADVANCE_INST(rINST, r9, 3) @ advance r9, update new rINST
    str     r2, [rSELF, #offThread_method]@ self->method = newSave->method
    ldr     r1, [r10, #offClassObject_pDvmDex]   @ r1<- method->clazz->pDvmDex
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = fp
#if defined(WITH_JIT)
    ldr     r10, [r0, #offStackSaveArea_returnAddr] @ r10 = saveArea->returnAddr
    mov     rPC, r9                     @ publish new rPC
    str     r1, [rSELF, #offThread_methodClassDex]
    str     r10, [rSELF, #offThread_inJitCodeCache]  @ may return to JIT'ed land
    cmp     r10, #0                      @ caller is compiled code
    blxne   r10
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
#else
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    mov     rPC, r9                     @ publish new rPC
    str     r1, [rSELF, #offThread_methodClassDex]
    GOTO_OPCODE(ip)                     @ jump to next instruction
#endif

19:
    @ Handle special actions
    @ On entry, r0: StackSaveArea
    ldr     r1, [r0, #offStackSaveArea_prevFrame]  @ r2<- prevFP
    str     rPC, [rSELF, #offThread_pc] @ update interpSave.pc
    str     r1, [rSELF, #offThread_curFrame]   @ update interpSave.curFrame
    mov     r0, rSELF
    bl      dvmReportReturn             @ (self)
    SAVEAREA_FROM_FP(r0, rFP)           @ restore StackSaveArea
    b       14b                         @ continue

    /*
     * Return handling, calls through "glue code".
     */
     .if    0
.LreturnOld:
    SAVE_PC_FP_TO_SELF()                @ export state
    mov     r0, rSELF                   @ arg to function
    bl      dvmMterp_returnFromMethod
    b       common_resumeAfterGlueCall
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

    mov     r0, rSELF
    bl      dvmCheckSuspendPending

    ldr     r9, [rSELF, #offThread_exception] @ r9<- self->exception
    mov     r1, rSELF                   @ r1<- self
    mov     r0, r9                      @ r0<- exception
    bl      dvmAddTrackedAlloc          @ don't let the exception be GCed
    ldrh    r2, [rSELF, #offThread_subMode]  @ get subMode flags
    mov     r3, #0                      @ r3<- NULL
    str     r3, [rSELF, #offThread_exception] @ self->exception = NULL

    @ Special subMode?
    cmp     r2, #0                      @ any special subMode handling needed?
    bne     7f                          @ go if so
8:
    /* set up args and a local for "&fp" */
    /* (str sp, [sp, #-4]!  would be perfect here, but is discouraged) */
    str     rFP, [sp, #-4]!             @ *--sp = fp
    mov     ip, sp                      @ ip<- &fp
    mov     r3, #0                      @ r3<- false
    str     ip, [sp, #-4]!              @ *--sp = &fp
    ldr     r1, [rSELF, #offThread_method] @ r1<- self->method
    mov     r0, rSELF                   @ r0<- self
    ldr     r1, [r1, #offMethod_insns]  @ r1<- method->insns
    mov     r2, r9                      @ r2<- exception
    sub     r1, rPC, r1                 @ r1<- pc - method->insns
    mov     r1, r1, asr #1              @ r1<- offset in code units

    /* call, r0 gets catchRelPc (a code-unit offset) */
    bl      dvmFindCatchBlock           @ call(self, relPc, exc, scan?, &fp)

    /* fix earlier stack overflow if necessary; may trash rFP */
    ldrb    r1, [rSELF, #offThread_stackOverflowed]
    cmp     r1, #0                      @ did we overflow earlier?
    beq     1f                          @ no, skip ahead
    mov     rFP, r0                     @ save relPc result in rFP
    mov     r0, rSELF                   @ r0<- self
    mov     r1, r9                      @ r1<- exception
    bl      dvmCleanupStackOverflow     @ call(self)
    mov     r0, rFP                     @ restore result
1:

    /* update frame pointer and check result from dvmFindCatchBlock */
    ldr     rFP, [sp, #4]               @ retrieve the updated rFP
    cmp     r0, #0                      @ is catchRelPc < 0?
    add     sp, sp, #8                  @ restore stack
    bmi     .LnotCaughtLocally

    /* adjust locals to match self->interpSave.curFrame and updated PC */
    SAVEAREA_FROM_FP(r1, rFP)           @ r1<- new save area
    ldr     r1, [r1, #offStackSaveArea_method] @ r1<- new method
    str     r1, [rSELF, #offThread_method]  @ self->method = new method
    ldr     r2, [r1, #offMethod_clazz]      @ r2<- method->clazz
    ldr     r3, [r1, #offMethod_insns]      @ r3<- method->insns
    ldr     r2, [r2, #offClassObject_pDvmDex] @ r2<- method->clazz->pDvmDex
    add     rPC, r3, r0, asl #1             @ rPC<- method->insns + catchRelPc
    str     r2, [rSELF, #offThread_methodClassDex] @ self->pDvmDex = meth...

    /* release the tracked alloc on the exception */
    mov     r0, r9                      @ r0<- exception
    mov     r1, rSELF                   @ r1<- self
    bl      dvmReleaseTrackedAlloc      @ release the exception

    /* restore the exception if the handler wants it */
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
    FETCH_INST()                        @ load rINST from rPC
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    cmp     ip, #OP_MOVE_EXCEPTION      @ is it "move-exception"?
    streq   r9, [rSELF, #offThread_exception] @ yes, restore the exception
    GOTO_OPCODE(ip)                     @ jump to next instruction

    @ Manage debugger bookkeeping
7:
    str     rPC, [rSELF, #offThread_pc]     @ update interpSave.pc
    str     rFP, [rSELF, #offThread_curFrame]     @ update interpSave.curFrame
    mov     r0, rSELF                       @ arg0<- self
    mov     r1, r9                          @ arg1<- exception
    bl      dvmReportExceptionThrow         @ (self, exception)
    b       8b                              @ resume with normal handling

.LnotCaughtLocally: @ r9=exception
    /* fix stack overflow if necessary */
    ldrb    r1, [rSELF, #offThread_stackOverflowed]
    cmp     r1, #0                      @ did we overflow earlier?
    movne   r0, rSELF                   @ if yes: r0<- self
    movne   r1, r9                      @ if yes: r1<- exception
    blne    dvmCleanupStackOverflow     @ if yes: call(self)

    @ may want to show "not caught locally" debug messages here
#if DVM_SHOW_EXCEPTION >= 2
    /* call __android_log_print(prio, tag, format, ...) */
    /* "Exception %s from %s:%d not caught locally" */
    @ dvmLineNumFromPC(method, pc - method->insns)
    ldr     r0, [rSELF, #offThread_method]
    ldr     r1, [r0, #offMethod_insns]
    sub     r1, rPC, r1
    asr     r1, r1, #1
    bl      dvmLineNumFromPC
    str     r0, [sp, #-4]!
    @ dvmGetMethodSourceFile(method)
    ldr     r0, [rSELF, #offThread_method]
    bl      dvmGetMethodSourceFile
    str     r0, [sp, #-4]!
    @ exception->clazz->descriptor
    ldr     r3, [r9, #offObject_clazz]
    ldr     r3, [r3, #offClassObject_descriptor]
    @
    ldr     r2, strExceptionNotCaughtLocally
0:  add     r2, pc
    ldr     r1, strLogTag
1:  add     r1, pc
    mov     r0, #3                      @ LOG_DEBUG
    bl      __android_log_print
#endif
    str     r9, [rSELF, #offThread_exception] @ restore exception
    mov     r0, r9                      @ r0<- exception
    mov     r1, rSELF                   @ r1<- self
    bl      dvmReleaseTrackedAlloc      @ release the exception
    b       common_gotoBail             @ bail out

strExceptionNotCaughtLocally:
    .word   PCREL_REF(.LstrExceptionNotCaughtLocally,0b)
strLogTag:
    .word   PCREL_REF(.LstrLogTag,1b)

    /*
     * Exception handling, calls through "glue code".
     */
    .if     0
.LexceptionOld:
    SAVE_PC_FP_TO_SELF()                @ export state
    mov     r0, rSELF                   @ arg to function
    bl      dvmMterp_exceptionThrown
    b       common_resumeAfterGlueCall
    .endif

#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including the current
     * instruction.
     *
     * On entry:
     *     r10: &dvmDex->pResFields[field]
     *     r0:  field pointer (must preserve)
     */
common_verifyField:
    ldrh    r3, [rSELF, #offThread_subMode]  @ r3 <- submode byte
    ands    r3, #kSubModeJitTraceBuild
    bxeq    lr                          @ Not building trace, continue
    ldr     r1, [r10]                   @ r1<- reload resolved StaticField ptr
    cmp     r1, #0                      @ resolution complete?
    bxne    lr                          @ yes, continue
    stmfd   sp!, {r0-r2,lr}             @ save regs
    mov     r0, rSELF
    mov     r1, rPC
    bl      dvmJitEndTraceSelect        @ (self,pc) end trace before this inst
    ldmfd   sp!, {r0-r2, lr}
    bx      lr                          @ return
#endif

/*
 * After returning from a "glued" function, pull out the updated
 * values and start executing at the next instruction.
 */
common_resumeAfterGlueCall:
    LOAD_PC_FP_FROM_SELF()              @ pull rPC and rFP out of thread
    ldr     rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh
    FETCH_INST()                        @ load rINST from rPC
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

/*
 * Invalid array index. Note that our calling convention is strange; we use r1
 * and r3 because those just happen to be the registers all our callers are
 * using. We move r3 before calling the C function, but r1 happens to match.
 * r1: index
 * r3: size
 */
common_errArrayIndex:
    EXPORT_PC()
    mov     r0, r3
    bl      dvmThrowArrayIndexOutOfBoundsException
    b       common_exceptionThrown

/*
 * Integer divide or mod by zero.
 */
common_errDivideByZero:
    EXPORT_PC()
    ldr     r0, strDivideByZero
0:  add     r0, pc
    bl      dvmThrowArithmeticException
    b       common_exceptionThrown

strDivideByZero:
    .word   PCREL_REF(.LstrDivideByZero,0b)

/*
 * Attempt to allocate an array with a negative size.
 * On entry: length in r1
 */
common_errNegativeArraySize:
    EXPORT_PC()
    mov     r0, r1                                @ arg0 <- len
    bl      dvmThrowNegativeArraySizeException    @ (len)
    b       common_exceptionThrown

/*
 * Invocation of a non-existent method.
 * On entry: method name in r1
 */
common_errNoSuchMethod:
    EXPORT_PC()
    mov     r0, r1
    bl      dvmThrowNoSuchMethodError
    b       common_exceptionThrown

/*
 * We encountered a null object when we weren't expecting one.  We
 * export the PC, throw a NullPointerException, and goto the exception
 * processing code.
 */
common_errNullObject:
    EXPORT_PC()
    mov     r0, #0
    bl      dvmThrowNullPointerException
    b       common_exceptionThrown

/*
 * For debugging, cause an immediate fault.  The source address will
 * be in lr (use a bl instruction to jump here).
 */
common_abort:
    ldr     pc, .LdeadFood
.LdeadFood:
    .word   0xdeadf00d

/*
 * Spit out a "we were here", preserving all registers.  (The attempt
 * to save ip won't work, but we need to save an even number of
 * registers for EABI 64-bit stack alignment.)
 */
    .macro  SQUEAK num
common_squeak\num:
    stmfd   sp!, {r0, r1, r2, r3, ip, lr}
    ldr     r0, strSqueak\num
0:  add     r0, pc
    mov     r1, #\num
    bl      printf
    ldmfd   sp!, {r0, r1, r2, r3, ip, lr}
    bx      lr

strSqueak\num:
    .word   PCREL_REF(.LstrSqueak,0b)
    .endm

    SQUEAK  0
    SQUEAK  1
    SQUEAK  2
    SQUEAK  3
    SQUEAK  4
    SQUEAK  5

/*
 * Spit out the number in r0, preserving registers.
 */
common_printNum:
    stmfd   sp!, {r0, r1, r2, r3, ip, lr}
    mov     r1, r0
    ldr     r0, strSqueak
0:  add     r0, pc
    bl      printf
    ldmfd   sp!, {r0, r1, r2, r3, ip, lr}
    bx      lr

strSqueak:
    .word   PCREL_REF(.LstrSqueak,0b)

/*
 * Print a newline, preserving registers.
 */
common_printNewline:
    stmfd   sp!, {r0, r1, r2, r3, ip, lr}
    ldr     r0, strNewline
0:  add     r0, pc
    bl      printf
    ldmfd   sp!, {r0, r1, r2, r3, ip, lr}
    bx      lr

strNewline:
    .word   PCREL_REF(.LstrNewline,0b)

    /*
     * Print the 32-bit quantity in r0 as a hex value, preserving registers.
     */
common_printHex:
    stmfd   sp!, {r0, r1, r2, r3, ip, lr}
    mov     r1, r0
    ldr     r0, strPrintHex
0:  add     r0, pc
    bl      printf
    ldmfd   sp!, {r0, r1, r2, r3, ip, lr}
    bx      lr

strPrintHex:
    .word   PCREL_REF(.LstrPrintHex,0b)

/*
 * Print the 64-bit quantity in r0-r1, preserving registers.
 */
common_printLong:
    stmfd   sp!, {r0, r1, r2, r3, ip, lr}
    mov     r3, r1
    mov     r2, r0
    ldr     r0, strPrintLong
0:  add     r0, pc
    bl      printf
    ldmfd   sp!, {r0, r1, r2, r3, ip, lr}
    bx      lr

strPrintLong:
    .word   PCREL_REF(.LstrPrintLong,0b)

/*
 * Print full method info.  Pass the Method* in r0.  Preserves regs.
 */
common_printMethod:
    stmfd   sp!, {r0, r1, r2, r3, ip, lr}
    bl      dvmMterpPrintMethod
    ldmfd   sp!, {r0, r1, r2, r3, ip, lr}
    bx      lr

/*
 * Call a C helper function that dumps regs and possibly some
 * additional info.  Requires the C function to be compiled in.
 */
    .if     0
common_dumpRegs:
    stmfd   sp!, {r0, r1, r2, r3, ip, lr}
    bl      dvmMterpDumpArmRegs
    ldmfd   sp!, {r0, r1, r2, r3, ip, lr}
    bx      lr
    .endif

#if 0
/*
 * Experiment on VFP mode.
 *
 * uint32_t setFPSCR(uint32_t val, uint32_t mask)
 *
 * Updates the bits specified by "mask", setting them to the values in "val".
 */
setFPSCR:
    and     r0, r0, r1                  @ make sure no stray bits are set
    fmrx    r2, fpscr                   @ get VFP reg
    mvn     r1, r1                      @ bit-invert mask
    and     r2, r2, r1                  @ clear masked bits
    orr     r2, r2, r0                  @ set specified bits
    fmxr    fpscr, r2                   @ set VFP reg
    mov     r0, r2                      @ return new value
    bx      lr

    .align  2
    .global dvmConfigureFP
    .type   dvmConfigureFP, %function
dvmConfigureFP:
    stmfd   sp!, {ip, lr}
    /* 0x03000000 sets DN/FZ */
    /* 0x00009f00 clears the six exception enable flags */
    bl      common_squeak0
    mov     r0, #0x03000000             @ r0<- 0x03000000
    add     r1, r0, #0x9f00             @ r1<- 0x03009f00
    bl      setFPSCR
    ldmfd   sp!, {ip, pc}
#endif



/*
 * Zero-terminated ASCII string data.
 *
 * On ARM we have two choices: do like gcc does, and LDR from a .word
 * with the address, or use an ADR pseudo-op to get the address
 * directly.  ADR saves 4 bytes and an indirection, but it's using a
 * PC-relative addressing mode and hence has a limited range, which
 * makes it not work well with mergeable string sections.
 */
    .section .rodata.str1.4,"aMS",%progbits,1

.LstrBadEntryPoint:
    .asciz  "Bad entry point %d\n"
.LstrFilledNewArrayNotImpl:
    .asciz  "filled-new-array only implemented for objects and 'int'"
.LstrDivideByZero:
    .asciz  "divide by zero"
.LstrLogTag:
    .asciz  "mterp"
.LstrExceptionNotCaughtLocally:
    .asciz  "Exception %s from %s:%d not caught locally\n"

.LstrNewline:
    .asciz  "\n"
.LstrSqueak:
    .asciz  "<%d>"
.LstrPrintHex:
    .asciz  "<%#x>"
.LstrPrintLong:
    .asciz  "<%lld>"

