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
 * This version supports the "new" ARM EABI.
 */

#include <machine/cpu-features.h>

#ifdef __ARM_EABI__

#ifdef EXTENDED_EABI_DEBUG
# define DBG
#else
# define DBG @
#endif


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
ARM EABI notes:

r0-r3 hold first 4 args to a method
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
64-bit quantities (long long, double) must be 64-bit aligned.  This means
we have to scan the method signature, identify arguments that must be
padded, and fix them up appropriately.
*/

    .text
    .align  2
    .global dvmPlatformInvoke
    .type   dvmPlatformInvoke, %function

/*
 * On entry:
 *   r0  JNIEnv (can be left alone)
 *   r1  clazz (NULL for virtual method calls, non-NULL for static)
 *   r2  arg info
 *   r3  argc (number of 32-bit values in argv)
 *   [sp]     argv
 *   [sp,#4]  short signature
 *   [sp,#8]  func
 *   [sp,#12] pReturn
 *
 * For a virtual method call, the "this" reference is in argv[0].
 *
 * argInfo (32-bit int) layout:
 *   SRRRLLLL FFFFFFFF FFFFFFFF FFFFFFFF
 *
 *   S - if set, do things the hard way (scan the signature)
 *   R - return-type enumeration, really only important for "hard" FP ABI
 *   L - number of double-words of storage required on stack (0-30 words)
 *   F - pad flag -- if set, write a pad word to the stack
 *
 * With this arrangement we can efficiently push up to 24 words of arguments
 * onto the stack.  Anything requiring more than that -- which should happen
 * rarely to never -- can do the slow signature scan.
 *
 * (We could pack the Fs more efficiently -- we know we never push two pads
 * in a row, and the first word can never be a pad -- but there's really
 * no need for it.)
 *
 * NOTE: if the called function has more than 4 words of arguments, gdb
 * will not be able to unwind the stack past this method.  The only way
 * around this is to convince gdb to respect an explicit frame pointer.
 * The stack unwinder in debuggerd *does* pay attention to fp if we set it
 * up appropriately, so at least that will work.
 */
dvmPlatformInvoke:
    .fnstart

    /*
     * Save regs.
     *
     * On entry to a function, "sp" must be 64-bit aligned.  This means
     * we have to adjust sp manually if we push an odd number of regs here
     * (both here and when exiting).
     *
     * The ARM spec doesn't specify anything about the frame pointer.  gcc
     * points fp at the first saved argument, so our "full descending"
     * stack looks like:
     *
     *  pReturn
     *  func
     *  shorty
     *  argv        <-- sp on entry
     *  lr          <-- fp
     *  fp
     *  r9...r7
     *  r6          <-- sp after reg save
     *
     * Any arguments that need to be pushed on for the target method
     * come after this.  The last argument is pushed first.
     */
SAVED_REG_COUNT = 6                     @ push 6 regs
FP_STACK_OFFSET = (SAVED_REG_COUNT-1) * 4 @ offset between fp and post-save sp
FP_ADJ = 4                              @ fp is initial sp +4

    .save        {r6, r7, r8, r9, fp, lr}
    stmfd   sp!, {r6, r7, r8, r9, fp, lr}

    .setfp  fp, sp, #FP_STACK_OFFSET    @ point fp at first saved reg
    add     fp, sp, #FP_STACK_OFFSET

    @.pad    #4                          @ adjust for 64-bit align
    @sub     sp, sp, #4                  @ (if we save odd number of regs)

    @ Ensure 64-bit alignment.  EABI guarantees sp is aligned on entry, make
    @ sure we're aligned properly now.
DBG tst     sp, #4                      @ 64-bit aligned?
DBG bne     dvmAbort                    @ no, fail

    ldr     r9, [fp, #0+FP_ADJ]         @ r9<- argv
    cmp     r1, #0                      @ calling a static method?

    @ Not static, grab the "this" pointer.  Note "this" is not explicitly
    @ described by the method signature.
    subeq   r3, r3, #1                  @ argc--
    ldreq   r1, [r9], #4                @ r1<- *argv++

    @ Do we have arg padding flags in "argInfo"? (just need to check hi bit)
    teq     r2, #0
    bmi     .Lno_arg_info

    /*
     * "Fast" path.
     *
     * Make room on the stack for the arguments and copy them over,
     * inserting pad words when appropriate.
     *
     * Currently:
     *  r0  don't touch
     *  r1  don't touch
     *  r2  arg info
     *  r3  argc
     *  r4-r5  don't touch (not saved)
     *  r6-r8 (available)
     *  r9  argv
     *  fp  frame pointer
     */
.Lhave_arg_info:
    @ Expand the stack by the specified amount.  We want to extract the
    @ count of double-words from r2, multiply it by 8, and subtract that
    @ from the stack pointer.
    and     ip, r2, #0x0f000000         @ ip<- double-words required
    mov     r6, r2, lsr #28             @ r6<- return type
    sub     sp, sp, ip, lsr #21         @ shift right 24, then left 3
    mov     r8, sp                      @ r8<- sp  (arg copy dest)

    @ Stick argv in r7 and advance it past the argv values that will be
    @ held in r2-r3.  It's possible r3 will hold a pad, so check the
    @ bit in r2.  We do this by ignoring the first bit (which would
    @ indicate a pad in r2) and shifting the second into the carry flag.
    @ If the carry is set, r3 will hold a pad, so we adjust argv less.
    @
    @ (This is harmless if argc==0)
    mov     r7, r9
    movs    r2, r2, lsr #2
    addcc   r7, r7, #8                  @ skip past 2 words, for r2 and r3
    subcc   r3, r3, #2
    addcs   r7, r7, #4                  @ skip past 1 word, for r2
    subcs   r3, r3, #1

.Lfast_copy_loop:
    @ if (--argc < 0) goto invoke
    subs    r3, r3, #1
    bmi     .Lcopy_done                 @ NOTE: expects original argv in r9

.Lfast_copy_loop2:
    @ Get pad flag into carry bit.  If it's set, we don't pull a value
    @ out of argv.
    movs    r2, r2, lsr #1
    ldrcc   ip, [r7], #4                @ ip = *r7++ (pull from argv)
    strcc   ip, [r8], #4                @ *r8++ = ip (write to stack)
    bcc     .Lfast_copy_loop

DBG movcs   ip, #-3                     @ DEBUG DEBUG - make pad word obvious
DBG strcs   ip, [r8]                    @ DEBUG DEBUG
    add     r8, r8, #4                  @ if pad, just advance ip without store
    b       .Lfast_copy_loop2           @ don't adjust argc after writing pad


.Lcopy_done:
    /*
     * Currently:
     *  r0-r3  args (JNIEnv*, thisOrClass, arg0, arg1)
     *  r6  return type (enum DalvikJniReturnType)
     *  r9  original argv
     *  fp  frame pointer
     *
     * The stack copy is complete.  Grab the first two words off of argv
     * and tuck them into r2/r3.  If the first arg is 32-bit and the second
     * arg is 64-bit, then r3 "holds" a pad word and the load is unnecessary
     * but harmless.
     *
     * If there are 0 or 1 arg words in argv, we will be loading uninitialized
     * data into the registers, but since nothing tries to use it it's also
     * harmless (assuming argv[0] and argv[1] point to valid memory, which
     * is a reasonable assumption for Dalvik's interpreted stacks).
     */
    ldmia   r9, {r2-r3}                 @ r2/r3<- argv[0]/argv[1]

    ldr     ip, [fp, #8+FP_ADJ]         @ ip<- func
#ifdef __ARM_HAVE_BLX
    blx     ip                          @ call func
#else
    mov     lr, pc                      @ call func the old-fashioned way
    bx      ip
#endif

    @ We're back, result is in r0 or (for long/double) r0-r1.
    @
    @ In theory, we need to use the "return type" arg to figure out what
    @ we have and how to return it.  However, unless we have an FPU and
    @ "hard" fp calling conventions, all we need to do is copy r0-r1 into
    @ the JValue union.
    @
    @ Thought: could redefine DalvikJniReturnType such that single-word
    @ and double-word values occupy different ranges; simple comparison
    @ allows us to choose between str and stm.  Probably not worthwhile.
    @
    cmp     r6, #0                      @ DALVIK_JNI_RETURN_VOID?
    ldrne   ip, [fp, #12+FP_ADJ]        @ pReturn
    sub     sp, fp, #FP_STACK_OFFSET    @ restore sp to post-reg-save offset
    stmneia ip, {r0-r1}                 @ pReturn->j <- r0/r1

    @ Restore the registers we saved and return.  On >= ARMv5TE we can
    @ restore PC directly from the saved LR.
#ifdef __ARM_HAVE_PC_INTERWORK
    ldmfd   sp!, {r6, r7, r8, r9, fp, pc}
#else
    ldmfd   sp!, {r6, r7, r8, r9, fp, lr}
    bx      lr
#endif



    /*
     * "Slow" path.
     * Walk through the argument list, counting up the number of 32-bit words
     * required to contain it.  Then walk through it a second time, copying
     * values out to the stack.  (We could pre-compute the size to save
     * ourselves a trip, but we'd have to store that somewhere -- this is
     * sufficiently unlikely that it's not worthwhile.)
     *
     * Try not to make any assumptions about the number of args -- I think
     * the class file format allows up to 64K words (need to verify that).
     *
     * Currently:
     *  r0  don't touch
     *  r1  don't touch
     *  r2  (available)
     *  r3  argc
     *  r4-r5 don't touch (not saved)
     *  r6-r8 (available)
     *  r9  argv
     *  fp  frame pointer
     */
.Lno_arg_info:
    mov     ip, r2, lsr #28             @ ip<- return type
    ldr     r6, [fp, #4+FP_ADJ]         @ r6<- short signature
    add     r6, r6, #1                  @ advance past return type
    mov     r2, #0                      @ r2<- word count, init to zero

.Lcount_loop:
    ldrb    ip, [r6], #1                @ ip<- *signature++
    cmp     ip, #0                      @ end?
    beq     .Lcount_done                @ all done, bail
    add     r2, r2, #1                  @ count++
    cmp     ip, #'D'                    @ look for 'D' or 'J', which are 64-bit
    cmpne   ip, #'J'
    bne     .Lcount_loop

    @ 64-bit value, insert padding if we're not aligned
    tst     r2, #1                      @ odd after initial incr?
    addne   r2, #1                      @ no, add 1 more to cover 64 bits
    addeq   r2, #2                      @ yes, treat prev as pad, incr 2 now
    b       .Lcount_loop
.Lcount_done:

    @ We have the padded-out word count in r2.  We subtract 2 from it
    @ because we don't push the first two arg words on the stack (they're
    @ destined for r2/r3).  Pushing them on and popping them off would be
    @ simpler but slower.
    subs    r2, r2, #2                  @ subtract 2 (for contents of r2/r3)
    movmis  r2, #0                      @ if negative, peg at zero, set Z-flag
    beq     .Lcopy_done                 @ zero args, skip stack copy

DBG tst     sp, #7                      @ DEBUG - make sure sp is aligned now
DBG bne     dvmAbort                    @ DEBUG

    @ Set up to copy from r7 to r8.  We copy from the second arg to the
    @ last arg, which means reading and writing to ascending addresses.
    sub     sp, sp, r2, asl #2          @ sp<- sp - r2*4
    bic     sp, #4                      @ subtract another 4 ifn
    mov     r7, r9                      @ r7<- argv
    mov     r8, sp                      @ r8<- sp

    @ We need to copy words from [r7] to [r8].  We walk forward through
    @ the signature again, "copying" pad words when appropriate, storing
    @ upward into the stack.
    ldr     r6, [fp, #4+FP_ADJ]         @ r6<- signature
    add     r6, r6, #1                  @ advance past return type
    add     r7, r7, #8                  @ r7<- r7+8 (assume argv 0/1 in r2/r3)

    @ Eat first arg or two, for the stuff that goes into r2/r3.
    ldrb    ip, [r6], #1                @ ip<- *signature++
    cmp     ip, #'D'
    cmpne   ip, #'J'
    beq     .Lstack_copy_loop           @ 64-bit arg fills r2+r3

    @ First arg was 32-bit, check the next
    ldrb    ip, [r6], #1                @ ip<- *signature++
    cmp     ip, #'D'
    cmpne   ip, #'J'
    subeq   r7, #4                      @ r7<- r7-4 (take it back - pad word)
    beq     .Lstack_copy_loop2          @ start with char we already have

    @ Two 32-bit args, fall through and start with next arg

.Lstack_copy_loop:
    ldrb    ip, [r6], #1                @ ip<- *signature++
.Lstack_copy_loop2:
    cmp     ip, #0                      @ end of shorty?
    beq     .Lcopy_done                 @ yes

    cmp     ip, #'D'
    cmpne   ip, #'J'
    beq     .Lcopy64

    @ Copy a 32-bit value.  [r8] is initially at the end of the stack.  We
    @ use "full descending" stacks, so we store into [r8] and incr as we
    @ move toward the end of the arg list.
.Lcopy32:
    ldr     ip, [r7], #4
    str     ip, [r8], #4
    b       .Lstack_copy_loop

.Lcopy64:
    @ Copy a 64-bit value.  If necessary, leave a hole in the stack to
    @ ensure alignment.  We know the [r8] output area is 64-bit aligned,
    @ so we can just mask the address.
    add     r8, r8, #7          @ r8<- (r8+7) & ~7
    ldr     ip, [r7], #4
    bic     r8, r8, #7
    ldr     r2, [r7], #4
    str     ip, [r8], #4
    str     r2, [r8], #4
    b       .Lstack_copy_loop

    .fnend
    .size   dvmPlatformInvoke, .-dvmPlatformInvoke

#if 0

/*
 * Spit out a "we were here", preserving all registers.  (The attempt
 * to save ip won't work, but we need to save an even number of
 * registers for EABI 64-bit stack alignment.)
 */
     .macro SQUEAK num
common_squeak\num:
    stmfd   sp!, {r0, r1, r2, r3, ip, lr}
    ldr     r0, strSqueak
    mov     r1, #\num
    bl      printf
#ifdef __ARM_HAVE_PC_INTERWORK
    ldmfd   sp!, {r0, r1, r2, r3, ip, pc}
#else
    ldmfd   sp!, {r0, r1, r2, r3, ip, lr}
    bx      lr
#endif
    .endm

    SQUEAK  0
    SQUEAK  1
    SQUEAK  2
    SQUEAK  3
    SQUEAK  4
    SQUEAK  5

strSqueak:
    .word   .LstrSqueak
.LstrSqueak:
    .asciz  "<%d>"

    .align  2

#endif

#endif /*__ARM_EABI__*/
