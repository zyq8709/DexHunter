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
 * JNI method invocation.  This is used to call a C/C++ JNI method.  The
 * argument list has to be pushed onto the native stack according to
 * local calling conventions.
 *
 * This version supports the "old" ARM ABI.
 */

#include <machine/cpu-features.h>

#ifndef __ARM_EABI__

/*
Function prototype:

void dvmPlatformInvoke(void* pEnv, ClassObject* clazz, int argInfo, int argc,
    const u4* argv, const char* signature, void* func, JValue* pReturn)

The method we are calling has the form:

  return_type func(JNIEnv* pEnv, ClassObject* clazz, ...)
    -or-
  return_type func(JNIEnv* pEnv, Object* this, ...)

We receive a collection of 32-bit values which correspond to arguments from
the interpreter (e.g. float occupies one, double occupies two).  It's up to
us to convert these into local calling conventions.
 */

/*
ARM ABI notes:

r0-r3 hold first 4 args to a method
r9 is given special treatment in some situations, but not for us
r10 (sl) seems to be generally available
r11 (fp) is used by gcc
r12 (ip) is scratch -- not preserved across method calls
r13 (sp) should be managed carefully in case a signal arrives
r14 (lr) must be preserved
r15 (pc) can be tinkered with directly

r0 holds returns <= 4 bytes
r0-r1 hold returns of 5-8 bytes, low word in r0

Stack is "full descending".  Only the arguments that don't fit in the first 4
registers are placed on the stack.  "sp" points at the first stacked argument
(i.e. the 5th arg).

VFP: single-precision results in s0, double-precision results in d0.

Happily we don't have to do anything special here -- the args from the
interpreter work directly as C/C++ args on ARM (with the "classic" ABI).
*/

    .text
    .align  2
    .global dvmPlatformInvoke
    .type   dvmPlatformInvoke, %function

/*
On entry:
  r0  JNIEnv
  r1  clazz (NULL for virtual method calls, non-NULL for static)
  r2  arg info (ignored)
  r3  argc
  [sp]     argv
  [sp,#4]  signature (ignored)
  [sp,#8]  func
  [sp,#12] pReturn
*/
dvmPlatformInvoke:
    @ Standard gcc stack frame setup.  We don't need to push the original
    @ sp or the current pc if "-fomit-frame-pointer" is in use for the
    @ rest of the code.  If we don't plan to use a debugger we can speed
    @ this up a little.
    mov     ip, sp
    stmfd   sp!, {r4, r5, r6, fp, ip, lr, pc}
    sub     fp, ip, #4          @ set up fp, same way gdb does

    @ We need to push a variable number of arguments onto the stack.
    @ Rather than keep a count and pop them off after, we just hold on to
    @ the stack pointers.
    @
    @ In theory we don't need to keep sp -- we can do an ldmdb instead of
    @ an ldmia -- but we're doing the gcc frame trick where we push the
    @ pc on with stmfd and don't pop it off.
    mov     r4, ip
    mov     r5, sp

    @ argc is already in a scratch register (r3).  Put argv into one.  Note
    @ argv can't go into r0-r3 because we need to use it to load those.
    ldr     ip, [r4, #0]        @ ip <-- argv

    @ Is this a static method?
    cmp     r1, #0

    @ No: set r1 to *argv++, and set argc--.
    @ (r0=pEnv, r1=this)
    ldreq   r1, [ip], #4
    subeq   r3, r3, #1

    @ While we still have the use of r2/r3, copy excess args from argv
    @ to the stack.  We need to push the last item in argv first, and we
    @ want the first two items in argv to end up in r2/r3.
    subs    r3, r3, #2
    ble     .Lno_copy

    @ If there are N args, we want to skip 0 and 1, and push (N-1)..2.  We
    @ have N-2 in r3.  If we set argv=argv+1, we can count from N-2 to 1
    @ inclusive and get the right set of args.
    add     r6, ip, #4

.Lcopy:
    @ *--sp = argv[count]
    ldr     r2, [r6, r3, lsl #2]
    str     r2, [sp, #-4]!
    subs    r3, r3, #1
    bne     .Lcopy

.Lno_copy:
    @ Load the last two args.  These are coming out of the interpreted stack,
    @ and the VM preserves an overflow region at the bottom, so it should be
    @ safe to load two items out of argv even if we're at the end.
    ldr     r2, [ip]
    ldr     r3, [ip, #4]

    @ Show time.  Tuck the pc into lr and load the pc from the method
    @ address supplied by the caller.  The value for "pc" is offset by 8
    @ due to instruction prefetching.
    @
#ifdef __ARM_HAVE_PC_INTERWORK
    mov     lr, pc
    ldr     pc, [r4, #8]
#else
    ldr     ip, [r4, #8]
    blx     ip
#endif

    @ We're back, result is in r0 or (for long/double) r0-r1.
    @
    @ In theory, we need to use the "return type" arg to figure out what
    @ we have and how to return it.  However, unless we have an FPU,
    @ all we need to do is copy r0-r1 into the JValue union.
    ldr     ip, [r4, #12]
    stmia   ip, {r0-r1}

#ifdef __ARM_HAVE_PC_INTERWORK
    @ Restore the registers we saved and return.  Note this remaps stuff,
    @ so that "sp" comes from "ip", "pc" comes from "lr", and the "pc"
    @ we pushed on evaporates when we restore "sp".
    ldmfd   r5, {r4, r5, r6, fp, sp, pc}
#else
    ldmfd   r5, {r4, r5, r6, fp, sp, lr}
    bx      lr
#endif

#endif /*__ARM_EABI__*/
