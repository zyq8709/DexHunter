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
#define GOTO_OPCODE(_reg)       add     pc, rIBASE, _reg, lsl #${handler_size_bits}
#define GOTO_OPCODE_BASE(_base,_reg)  add     pc, _base, _reg, lsl #${handler_size_bits}
#define GOTO_OPCODE_IFEQ(_reg)  addeq   pc, rIBASE, _reg, lsl #${handler_size_bits}
#define GOTO_OPCODE_IFNE(_reg)  addne   pc, rIBASE, _reg, lsl #${handler_size_bits}

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
