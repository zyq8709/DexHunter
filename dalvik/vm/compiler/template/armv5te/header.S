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

Callee must save/restore r4+ (except r12) if it modifies them.

Stack is "full descending".  Only the arguments that don't fit in the first 4
registers are placed on the stack.  "sp" points at the first stacked argument
(i.e. the 5th arg).

VFP: single-precision results in s0, double-precision results in d0.

In the EABI, "sp" must be 64-bit aligned on entry to a function, and any
64-bit quantities (long long, double) must be 64-bit aligned.
*/

/*
JIT and ARM notes:

The following registers have fixed assignments:

  reg nick      purpose
  r5  rFP       interpreted frame pointer, used for accessing locals and args
  r6  rSELF     thread pointer

The following registers have fixed assignments in mterp but are scratch
registers in compiled code

  reg nick      purpose
  r4  rPC       interpreted program counter, used for fetching instructions
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

/*
 * Given a frame pointer, find the stack save area.
 *
 * In C this is "((StackSaveArea*)(_fp) -1)".
 */
#define SAVEAREA_FROM_FP(_reg, _fpreg) \
    sub     _reg, _fpreg, #sizeofStackSaveArea

#define EXPORT_PC() \
    str     rPC, [rFP, #(-sizeofStackSaveArea + offStackSaveArea_currentPc)]

/*
 * This is a #include, not a %include, because we want the C pre-processor
 * to expand the macros into assembler assignment statements.
 */
#include "../../../mterp/common/asm-constants.h"
