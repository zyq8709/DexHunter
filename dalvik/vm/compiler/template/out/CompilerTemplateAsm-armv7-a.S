/*
 * This file was generated automatically by gen-template.py for 'armv7-a'.
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

/* File: armv5te-vfp/platform.S */
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
/* File: armv5te/TEMPLATE_CMP_LONG.S */
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
    cmp     r1, r3                      @ compare (vBB+1, vCC+1)
    blt     .LTEMPLATE_CMP_LONG_less            @ signed compare on high part
    bgt     .LTEMPLATE_CMP_LONG_greater
    subs    r0, r0, r2                  @ r0<- r0 - r2
    bxeq     lr
    bhi     .LTEMPLATE_CMP_LONG_greater         @ unsigned compare on low part
.LTEMPLATE_CMP_LONG_less:
    mvn     r0, #0                      @ r0<- -1
    bx      lr
.LTEMPLATE_CMP_LONG_greater:
    mov     r0, #1                      @ r0<- 1
    bx      lr

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_RETURN
dvmCompiler_TEMPLATE_RETURN:
/* File: armv5te/TEMPLATE_RETURN.S */
    /*
     * Unwind a frame from the Dalvik stack for compiled OP_RETURN_XXX.
     * If the stored value in returnAddr
     * is non-zero, the caller is compiled by the JIT thus return to the
     * address in the code cache following the invoke instruction. Otherwise
     * return to the special dvmJitToInterpNoChain entry point.
     */
#if defined(TEMPLATE_INLINE_PROFILING)
    stmfd   sp!, {r0-r2,lr}             @ preserve live registers
    mov     r0, r6
    @ r0=rSELF
    ldr     ip, .LdvmFastMethodTraceExit
    blx     ip
    ldmfd   sp!, {r0-r2,lr}             @ restore live registers
#endif
    SAVEAREA_FROM_FP(r0, rFP)           @ r0<- saveArea (old)
    ldr     r10, [r0, #offStackSaveArea_prevFrame] @ r10<- saveArea->prevFrame
    ldrb    r8, [rSELF, #offThread_breakFlags] @ r8<- breakFlags
    ldr     rPC, [r0, #offStackSaveArea_savedPc] @ rPC<- saveArea->savedPc
    ldr     r2, [r10, #(offStackSaveArea_method - sizeofStackSaveArea)]
#if !defined(WITH_SELF_VERIFICATION)
    ldr     r9,  [r0, #offStackSaveArea_returnAddr] @ r9<- chaining cell ret
#else
    mov     r9, #0                      @ disable chaining
#endif
                                        @ r2<- method we're returning to
    cmp     r2, #0                      @ break frame?
#if !defined(WITH_SELF_VERIFICATION)
    beq     1f                          @ bail to interpreter
#else
    blxeq   lr                          @ punt to interpreter and compare state
#endif
    ldr     r1, .LdvmJitToInterpNoChainNoProfile @ defined in footer.S
    mov     rFP, r10                    @ publish new FP
    ldr     r10, [r2, #offMethod_clazz] @ r10<- method->clazz

    str     r2, [rSELF, #offThread_method]@ self->method = newSave->method
    ldr     r0, [r10, #offClassObject_pDvmDex] @ r0<- method->clazz->pDvmDex
    str     rFP, [rSELF, #offThread_curFrame] @ curFrame = fp
    add     rPC, rPC, #6                @ publish new rPC (advance 6 bytes)
    str     r0, [rSELF, #offThread_methodClassDex]
    cmp     r8, #0                      @ check the break flags
    movne   r9, #0                      @ clear the chaining cell address
    str     r9, [rSELF, #offThread_inJitCodeCache] @ in code cache or not
    cmp     r9, #0                      @ chaining cell exists?
    blxne   r9                          @ jump to the chaining cell
#if defined(WITH_JIT_TUNING)
    mov     r0, #kCallsiteInterpreted
#endif
    mov     pc, r1                      @ callsite is interpreted
1:
    mov     r0, #0
    str     r0, [rSELF, #offThread_inJitCodeCache] @ reset inJitCodeCache
    stmia   rSELF, {rPC, rFP}           @ SAVE_PC_FP_TO_SELF()
    ldr     r2, .LdvmMterpStdBail       @ defined in footer.S
    mov     r0, rSELF                   @ Expecting rSELF in r0
    blx     r2                          @ exit the interpreter

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_NO_OPT
dvmCompiler_TEMPLATE_INVOKE_METHOD_NO_OPT:
/* File: armv5te/TEMPLATE_INVOKE_METHOD_NO_OPT.S */
    /*
     * For polymorphic callsites - setup the Dalvik frame and load Dalvik PC
     * into rPC then jump to dvmJitToInterpNoChain to dispatch the
     * runtime-resolved callee.
     */
    @ r0 = methodToCall, r1 = returnCell, rPC = dalvikCallsite
    ldrh    r7, [r0, #offMethod_registersSize]  @ r7<- methodToCall->regsSize
    ldrh    r2, [r0, #offMethod_outsSize]  @ r2<- methodToCall->outsSize
    ldr     r9, [rSELF, #offThread_interpStackEnd]    @ r9<- interpStackEnd
    ldrb    r8, [rSELF, #offThread_breakFlags] @ r8<- breakFlags
    add     r3, r1, #1  @ Thumb addr is odd
    SAVEAREA_FROM_FP(r1, rFP)           @ r1<- stack save area
    sub     r1, r1, r7, lsl #2          @ r1<- newFp (old savearea - regsSize)
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- stack save area
    sub     r10, r10, r2, lsl #2        @ r10<- bottom (newsave - outsSize)
    cmp     r10, r9                     @ bottom < interpStackEnd?
    bxlo    lr                          @ return to raise stack overflow excep.
    @ r1 = newFP, r0 = methodToCall, r3 = returnCell, rPC = dalvikCallsite
    ldr     r9, [r0, #offMethod_clazz]      @ r9<- method->clazz
    ldr     r10, [r0, #offMethod_accessFlags] @ r10<- methodToCall->accessFlags
    str     rPC, [rFP, #(offStackSaveArea_currentPc - sizeofStackSaveArea)]
    str     rPC, [r1, #(offStackSaveArea_savedPc - sizeofStackSaveArea)]
    ldr     rPC, [r0, #offMethod_insns]     @ rPC<- methodToCall->insns


    @ set up newSaveArea
    str     rFP, [r1, #(offStackSaveArea_prevFrame - sizeofStackSaveArea)]
    str     r3, [r1, #(offStackSaveArea_returnAddr - sizeofStackSaveArea)]
    str     r0, [r1, #(offStackSaveArea_method - sizeofStackSaveArea)]
    cmp     r8, #0                      @ breakFlags != 0
    bxne    lr                          @ bail to the interpreter
    tst     r10, #ACC_NATIVE
#if !defined(WITH_SELF_VERIFICATION)
    bne     .LinvokeNative
#else
    bxne    lr                          @ bail to the interpreter
#endif

    ldr     r10, .LdvmJitToInterpTraceSelectNoChain
    ldr     r3, [r9, #offClassObject_pDvmDex] @ r3<- method->clazz->pDvmDex

    @ Update "thread" values for the new method
    str     r0, [rSELF, #offThread_method]    @ self->method = methodToCall
    str     r3, [rSELF, #offThread_methodClassDex] @ self->methodClassDex = ...
    mov     rFP, r1                         @ fp = newFp
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = newFp
#if defined(TEMPLATE_INLINE_PROFILING)
    stmfd   sp!, {r0-r3}                    @ preserve r0-r3
    mov     r1, r6
    @ r0=methodToCall, r1=rSELF
    ldr     ip, .LdvmFastMethodTraceEnter
    blx     ip
    ldmfd   sp!, {r0-r3}                    @ restore r0-r3
#endif

    @ Start executing the callee
#if defined(WITH_JIT_TUNING)
    mov     r0, #kInlineCacheMiss
#endif
    bx      r10                         @ dvmJitToInterpTraceSelectNoChain

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_CHAIN
dvmCompiler_TEMPLATE_INVOKE_METHOD_CHAIN:
/* File: armv5te/TEMPLATE_INVOKE_METHOD_CHAIN.S */
    /*
     * For monomorphic callsite, setup the Dalvik frame and return to the
     * Thumb code through the link register to transfer control to the callee
     * method through a dedicated chaining cell.
     */
    @ r0 = methodToCall, r1 = returnCell, r2 = methodToCall->outsSize
    @ rPC = dalvikCallsite, r7 = methodToCall->registersSize
    @ methodToCall is guaranteed to be non-native
.LinvokeChain:
    ldr     r9, [rSELF, #offThread_interpStackEnd]    @ r9<- interpStackEnd
    ldrb    r8, [rSELF, #offThread_breakFlags]        @ r8<- breakFlags
    add     r3, r1, #1  @ Thumb addr is odd
    SAVEAREA_FROM_FP(r1, rFP)           @ r1<- stack save area
    sub     r1, r1, r7, lsl #2          @ r1<- newFp (old savearea - regsSize)
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- stack save area
    add     r12, lr, #2                 @ setup the punt-to-interp address
    sub     r10, r10, r2, lsl #2        @ r10<- bottom (newsave - outsSize)
    cmp     r10, r9                     @ bottom < interpStackEnd?
    bxlo    r12                         @ return to raise stack overflow excep.
    @ r1 = newFP, r0 = methodToCall, r3 = returnCell, rPC = dalvikCallsite
    ldr     r9, [r0, #offMethod_clazz]      @ r9<- method->clazz
    str     rPC, [rFP, #(offStackSaveArea_currentPc - sizeofStackSaveArea)]
    str     rPC, [r1, #(offStackSaveArea_savedPc - sizeofStackSaveArea)]

    @ set up newSaveArea
    str     rFP, [r1, #(offStackSaveArea_prevFrame - sizeofStackSaveArea)]
    str     r3, [r1, #(offStackSaveArea_returnAddr - sizeofStackSaveArea)]
    str     r0, [r1, #(offStackSaveArea_method - sizeofStackSaveArea)]
    cmp     r8, #0                      @ breakFlags != 0
    bxne    r12                         @ bail to the interpreter

    ldr     r3, [r9, #offClassObject_pDvmDex] @ r3<- method->clazz->pDvmDex

    @ Update "thread" values for the new method
    str     r0, [rSELF, #offThread_method]    @ self->method = methodToCall
    str     r3, [rSELF, #offThread_methodClassDex] @ self->methodClassDex = ...
    mov     rFP, r1                         @ fp = newFp
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = newFp
#if defined(TEMPLATE_INLINE_PROFILING)
    stmfd   sp!, {r0-r2,lr}             @ preserve clobbered live registers
    mov     r1, r6
    @ r0=methodToCall, r1=rSELF
    ldr     ip, .LdvmFastMethodTraceEnter
    blx     ip
    ldmfd   sp!, {r0-r2,lr}             @ restore registers
#endif

    bx      lr                              @ return to the callee-chaining cell

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN
dvmCompiler_TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN:
/* File: armv5te/TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN.S */
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
     *      const ClassObject *clazz;
     *      const Method *method;
     *      u4 counter;
     *  } PredictedChainingCell;
     *
     * Upon returning to the callsite:
     *    - lr  : to branch to the chaining cell
     *    - lr+2: to punt to the interpreter
     *    - lr+4: to fully resolve the callee and may rechain.
     *            r3 <- class
     *            r9 <- counter
     */
    @ r0 = this, r1 = returnCell, r2 = predictedChainCell, rPC = dalvikCallsite
    ldr     r3, [r0, #offObject_clazz]  @ r3 <- this->class
    ldr     r8, [r2, #4]    @ r8 <- predictedChainCell->clazz
    ldr     r0, [r2, #8]    @ r0 <- predictedChainCell->method
    ldr     r9, [rSELF, #offThread_icRechainCount] @ r1 <- shared rechainCount
    cmp     r3, r8          @ predicted class == actual class?
#if defined(WITH_JIT_TUNING)
    ldr     r7, .LdvmICHitCount
#if defined(WORKAROUND_CORTEX_A9_745320)
    /* Don't use conditional loads if the HW defect exists */
    bne     101f
    ldr     r10, [r7, #0]
101:
#else
    ldreq   r10, [r7, #0]
#endif
    add     r10, r10, #1
    streq   r10, [r7, #0]
#endif
    ldreqh  r7, [r0, #offMethod_registersSize]  @ r7<- methodToCall->regsSize
    ldreqh  r2, [r0, #offMethod_outsSize]  @ r2<- methodToCall->outsSize
    beq     .LinvokeChain   @ predicted chain is valid
    ldr     r7, [r3, #offClassObject_vtable] @ r7 <- this->class->vtable
    cmp     r8, #0          @ initialized class or not
    moveq   r1, #0
    subne   r1, r9, #1      @ count--
    strne   r1, [rSELF, #offThread_icRechainCount]  @ write back to thread
    add     lr, lr, #4      @ return to fully-resolve landing pad
    /*
     * r1 <- count
     * r2 <- &predictedChainCell
     * r3 <- this->class
     * r4 <- dPC
     * r7 <- this->class->vtable
     */
    bx      lr

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_NATIVE
dvmCompiler_TEMPLATE_INVOKE_METHOD_NATIVE:
/* File: armv5te/TEMPLATE_INVOKE_METHOD_NATIVE.S */
    @ r0 = methodToCall, r1 = returnCell, rPC = dalvikCallsite
    @ r7 = methodToCall->registersSize
    ldr     r9, [rSELF, #offThread_interpStackEnd]    @ r9<- interpStackEnd
    ldrb    r8, [rSELF, #offThread_breakFlags]        @ r8<- breakFlags
    add     r3, r1, #1  @ Thumb addr is odd
    SAVEAREA_FROM_FP(r1, rFP)           @ r1<- stack save area
    sub     r1, r1, r7, lsl #2          @ r1<- newFp (old savearea - regsSize)
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- stack save area
    cmp     r10, r9                     @ bottom < interpStackEnd?
    bxlo    lr                          @ return to raise stack overflow excep.
    @ r1 = newFP, r0 = methodToCall, r3 = returnCell, rPC = dalvikCallsite
    str     rPC, [rFP, #(offStackSaveArea_currentPc - sizeofStackSaveArea)]
    str     rPC, [r1, #(offStackSaveArea_savedPc - sizeofStackSaveArea)]

    @ set up newSaveArea
    str     rFP, [r1, #(offStackSaveArea_prevFrame - sizeofStackSaveArea)]
    str     r3, [r1, #(offStackSaveArea_returnAddr - sizeofStackSaveArea)]
    str     r0, [r1, #(offStackSaveArea_method - sizeofStackSaveArea)]
    cmp     r8, #0                      @ breakFlags != 0
    ldr     r8, [r0, #offMethod_nativeFunc] @ r8<- method->nativeFunc
#if !defined(WITH_SELF_VERIFICATION)
    bxne    lr                          @ bail to the interpreter
#else
    bx      lr                          @ bail to interpreter unconditionally
#endif

    @ go ahead and transfer control to the native code
    ldr     r9, [rSELF, #offThread_jniLocal_topCookie]@r9<-thread->localRef->...
    mov     r2, #0
    str     r1, [rSELF, #offThread_curFrame]   @ curFrame = newFp
    str     r2, [rSELF, #offThread_inJitCodeCache] @ not in the jit code cache
    str     r9, [r1, #(offStackSaveArea_localRefCookie - sizeofStackSaveArea)]
                                        @ newFp->localRefCookie=top
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- new stack save area

    mov     r2, r0                        @ arg2<- methodToCall
    mov     r0, r1                        @ arg0<- newFP
    add     r1, rSELF, #offThread_retval  @ arg1<- &retval
    mov     r3, rSELF                     @ arg3<- self
#if defined(TEMPLATE_INLINE_PROFILING)
    @ r2=methodToCall, r6=rSELF
    stmfd   sp!, {r2,r6}                @ to be consumed after JNI return
    stmfd   sp!, {r0-r3}                @ preserve r0-r3
    mov     r0, r2
    mov     r1, r6
    @ r0=JNIMethod, r1=rSELF
    ldr     ip, .LdvmFastMethodTraceEnter
    blx     ip
    ldmfd   sp!, {r0-r3}                @ restore r0-r3
#endif

    blx     r8                          @ off to the native code

#if defined(TEMPLATE_INLINE_PROFILING)
    ldmfd   sp!, {r0-r1}                @ restore r2 and r6
    @ r0=JNIMethod, r1=rSELF
    ldr     ip, .LdvmFastNativeMethodTraceExit
    blx     ip
#endif
    @ native return; r10=newSaveArea
    @ equivalent to dvmPopJniLocals
    ldr     r2, [r10, #offStackSaveArea_returnAddr] @ r2 = chaining cell ret
    ldr     r0, [r10, #offStackSaveArea_localRefCookie] @ r0<- saved->top
    ldr     r1, [rSELF, #offThread_exception] @ check for exception
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = fp
    cmp     r1, #0                      @ null?
    str     r0, [rSELF, #offThread_jniLocal_topCookie] @ new top <- old top
    ldr     r0, [rFP, #(offStackSaveArea_currentPc - sizeofStackSaveArea)]

    @ r0 = dalvikCallsitePC
    bne     .LhandleException           @ no, handle exception

    str     r2, [rSELF, #offThread_inJitCodeCache] @ set the mode properly
    cmp     r2, #0                      @ return chaining cell still exists?
    bxne    r2                          @ yes - go ahead

    @ continue executing the next instruction through the interpreter
    ldr     r1, .LdvmJitToInterpTraceSelectNoChain @ defined in footer.S
    add     rPC, r0, #6                 @ reconstruct new rPC (advance 6 bytes)
#if defined(WITH_JIT_TUNING)
    mov     r0, #kCallsiteInterpreted
#endif
    mov     pc, r1

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_MUL_LONG
dvmCompiler_TEMPLATE_MUL_LONG:
/* File: armv5te/TEMPLATE_MUL_LONG.S */
    /*
     * Signed 64-bit integer multiply.
     *
     * For JIT: op1 in r0/r1, op2 in r2/r3, return in r0/r1
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
    mul     ip, r2, r1                  @  ip<- ZxW
    umull   r9, r10, r2, r0             @  r9/r10 <- ZxX
    mla     r2, r0, r3, ip              @  r2<- YxX + (ZxW)
    mov     r0, r9
    add     r1, r2, r10                 @  r1<- r10 + low(ZxW + (YxX))
    bx      lr

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_SHL_LONG
dvmCompiler_TEMPLATE_SHL_LONG:
/* File: armv5te/TEMPLATE_SHL_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     */
    /* shl-long vAA, vBB, vCC */
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    mov     r1, r1, asl r2              @  r1<- r1 << r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r1, r1, r0, lsr r3          @  r1<- r1 | (r0 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r1, r0, asl ip              @  if r2 >= 32, r1<- r0 << (r2-32)
    mov     r0, r0, asl r2              @  r0<- r0 << r2
    bx      lr

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_SHR_LONG
dvmCompiler_TEMPLATE_SHR_LONG:
/* File: armv5te/TEMPLATE_SHR_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     */
    /* shr-long vAA, vBB, vCC */
    and     r2, r2, #63                 @ r0<- r0 & 0x3f
    mov     r0, r0, lsr r2              @  r0<- r2 >> r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @  r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r0, r1, asr ip              @  if r2 >= 32, r0<-r1 >> (r2-32)
    mov     r1, r1, asr r2              @  r1<- r1 >> r2
    bx      lr

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_USHR_LONG
dvmCompiler_TEMPLATE_USHR_LONG:
/* File: armv5te/TEMPLATE_USHR_LONG.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     */
    /* ushr-long vAA, vBB, vCC */
    and     r2, r2, #63                 @ r0<- r0 & 0x3f
    mov     r0, r0, lsr r2              @  r0<- r2 >> r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @  r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r0, r1, lsr ip              @  if r2 >= 32, r0<-r1 >>> (r2-32)
    mov     r1, r1, lsr r2              @  r1<- r1 >>> r2
    bx      lr

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_THROW_EXCEPTION_COMMON
dvmCompiler_TEMPLATE_THROW_EXCEPTION_COMMON:
/* File: armv5te/TEMPLATE_THROW_EXCEPTION_COMMON.S */
    /*
     * Throw an exception from JIT'ed code.
     * On entry:
     *    r0    Dalvik PC that raises the exception
     */
    b       .LhandleException

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_MEM_OP_DECODE
dvmCompiler_TEMPLATE_MEM_OP_DECODE:
/* File: armv5te-vfp/TEMPLATE_MEM_OP_DECODE.S */
#if defined(WITH_SELF_VERIFICATION)
    /*
     * This handler encapsulates heap memory ops for selfVerification mode.
     *
     * The call to the handler is inserted prior to a heap memory operation.
     * This handler then calls a function to decode the memory op, and process
     * it accordingly. Afterwards, the handler changes the return address to
     * skip the memory op so it never gets executed.
     */
    vpush   {d0-d15}                    @ save out all fp registers
    push    {r0-r12,lr}                 @ save out all registers
    ldr     r2, .LdvmSelfVerificationMemOpDecode @ defined in footer.S
    mov     r0, lr                      @ arg0 <- link register
    mov     r1, sp                      @ arg1 <- stack pointer
    blx     r2                          @ decode and handle the mem op
    pop     {r0-r12,lr}                 @ restore all registers
    vpop    {d0-d15}                    @ restore all fp registers
    bx      lr                          @ return to compiled code
#endif

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_STRING_COMPARETO
dvmCompiler_TEMPLATE_STRING_COMPARETO:
/* File: armv5te/TEMPLATE_STRING_COMPARETO.S */
    /*
     * String's compareTo.
     *
     * Requires r0/r1 to have been previously checked for null.  Will
     * return negative if this's string is < comp, 0 if they are the
     * same and positive if >.
     *
     * IMPORTANT NOTE:
     *
     * This code relies on hard-coded offsets for string objects, and must be
     * kept in sync with definitions in UtfString.h.  See asm-constants.h
     *
     * On entry:
     *    r0:   this object pointer
     *    r1:   comp object pointer
     *
     */

    mov    r2, r0         @ this to r2, opening up r0 for return value
    subs   r0, r2, r1     @ Same?
    bxeq   lr

    ldr    r4, [r2, #STRING_FIELDOFF_OFFSET]
    ldr    r9, [r1, #STRING_FIELDOFF_OFFSET]
    ldr    r7, [r2, #STRING_FIELDOFF_COUNT]
    ldr    r10, [r1, #STRING_FIELDOFF_COUNT]
    ldr    r2, [r2, #STRING_FIELDOFF_VALUE]
    ldr    r1, [r1, #STRING_FIELDOFF_VALUE]

    /*
     * At this point, we have:
     *    value:  r2/r1
     *    offset: r4/r9
     *    count:  r7/r10
     * We're going to compute
     *    r11 <- countDiff
     *    r10 <- minCount
     */
     subs  r11, r7, r10
     movls r10, r7

     /* Now, build pointers to the string data */
     add   r2, r2, r4, lsl #1
     add   r1, r1, r9, lsl #1
     /*
      * Note: data pointers point to previous element so we can use pre-index
      * mode with base writeback.
      */
     add   r2, #16-2   @ offset to contents[-1]
     add   r1, #16-2   @ offset to contents[-1]

     /*
      * At this point we have:
      *   r2: *this string data
      *   r1: *comp string data
      *   r10: iteration count for comparison
      *   r11: value to return if the first part of the string is equal
      *   r0: reserved for result
      *   r3, r4, r7, r8, r9, r12 available for loading string data
      */

    subs  r10, #2
    blt   do_remainder2

      /*
       * Unroll the first two checks so we can quickly catch early mismatch
       * on long strings (but preserve incoming alignment)
       */

    ldrh  r3, [r2, #2]!
    ldrh  r4, [r1, #2]!
    ldrh  r7, [r2, #2]!
    ldrh  r8, [r1, #2]!
    subs  r0, r3, r4
    subeqs  r0, r7, r8
    bxne  lr
    cmp   r10, #28
    bgt   do_memcmp16
    subs  r10, #3
    blt   do_remainder

loopback_triple:
    ldrh  r3, [r2, #2]!
    ldrh  r4, [r1, #2]!
    ldrh  r7, [r2, #2]!
    ldrh  r8, [r1, #2]!
    ldrh  r9, [r2, #2]!
    ldrh  r12,[r1, #2]!
    subs  r0, r3, r4
    subeqs  r0, r7, r8
    subeqs  r0, r9, r12
    bxne  lr
    subs  r10, #3
    bge   loopback_triple

do_remainder:
    adds  r10, #3
    beq   returnDiff

loopback_single:
    ldrh  r3, [r2, #2]!
    ldrh  r4, [r1, #2]!
    subs  r0, r3, r4
    bxne  lr
    subs  r10, #1
    bne     loopback_single

returnDiff:
    mov   r0, r11
    bx    lr

do_remainder2:
    adds  r10, #2
    bne   loopback_single
    mov   r0, r11
    bx    lr

    /* Long string case */
do_memcmp16:
    mov   r4, lr
    ldr   lr, .Lmemcmp16
    mov   r7, r11
    add   r0, r2, #2
    add   r1, r1, #2
    mov   r2, r10
    blx   lr
    cmp   r0, #0
    bxne  r4
    mov   r0, r7
    bx    r4

.Lmemcmp16:
    .word __memcmp16

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_STRING_INDEXOF
dvmCompiler_TEMPLATE_STRING_INDEXOF:
/* File: armv5te/TEMPLATE_STRING_INDEXOF.S */
    /*
     * String's indexOf.
     *
     * Requires r0 to have been previously checked for null.  Will
     * return index of match of r1 in r0.
     *
     * IMPORTANT NOTE:
     *
     * This code relies on hard-coded offsets for string objects, and must be
     * kept in sync wth definitions in UtfString.h  See asm-constants.h
     *
     * On entry:
     *    r0:   string object pointer
     *    r1:   char to match
     *    r2:   Starting offset in string data
     */

    ldr    r3, [r0, #STRING_FIELDOFF_VALUE]
    ldr    r7, [r0, #STRING_FIELDOFF_OFFSET]
    ldr    r8, [r0, #STRING_FIELDOFF_COUNT]


    /*
     * At this point, we have:
     *    r1: char to match
     *    r2: starting offset
     *    r3: object pointer (final result -> r0)
     *    r7: offset
     *    r8: string length
     */

     /* Build pointer to start of string data */
     add   r3, #16
     add   r0, r3, r7, lsl #1

     /* Save a copy of starting data in r7 */
     mov   r7, r0

     /* Clamp start to [0..count] */
     cmp   r2, #0
     movlt r2, #0
     cmp   r2, r8
     movgt r2, r8

     /* Build pointer to start of data to compare and pre-bias */
     add   r0, r0, r2, lsl #1
     sub   r0, #2

     /* Compute iteration count */
     sub   r8, r2

     /*
      * At this point we have:
      *   r0: start of data to test
      *   r1: chat to compare
      *   r8: iteration count
      *   r7: original start of string
      *   r3, r4, r9, r10, r11, r12 available for loading string data
      */

    subs  r8, #4
    blt   indexof_remainder

indexof_loop4:
    ldrh  r3, [r0, #2]!
    ldrh  r4, [r0, #2]!
    ldrh  r10, [r0, #2]!
    ldrh  r11, [r0, #2]!
    cmp   r3, r1
    beq   match_0
    cmp   r4, r1
    beq   match_1
    cmp   r10, r1
    beq   match_2
    cmp   r11, r1
    beq   match_3
    subs  r8, #4
    bge   indexof_loop4

indexof_remainder:
    adds    r8, #4
    beq     indexof_nomatch

indexof_loop1:
    ldrh  r3, [r0, #2]!
    cmp   r3, r1
    beq   match_3
    subs  r8, #1
    bne   indexof_loop1

indexof_nomatch:
    mov   r0, #-1
    bx    lr

match_0:
    sub   r0, #6
    sub   r0, r7
    asr   r0, r0, #1
    bx    lr
match_1:
    sub   r0, #4
    sub   r0, r7
    asr   r0, r0, #1
    bx    lr
match_2:
    sub   r0, #2
    sub   r0, r7
    asr   r0, r0, #1
    bx    lr
match_3:
    sub   r0, r7
    asr   r0, r0, #1
    bx    lr

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INTERPRET
dvmCompiler_TEMPLATE_INTERPRET:
/* File: armv5te/TEMPLATE_INTERPRET.S */
    /*
     * This handler transfers control to the interpeter without performing
     * any lookups.  It may be called either as part of a normal chaining
     * operation, or from the transition code in header.S.  We distinquish
     * the two cases by looking at the link register.  If called from a
     * translation chain, it will point to the chaining Dalvik PC -3.
     * On entry:
     *    lr - if NULL:
     *        r1 - the Dalvik PC to begin interpretation.
     *    else
     *        [lr, #3] contains Dalvik PC to begin interpretation
     *    rSELF - pointer to thread
     *    rFP - Dalvik frame pointer
     */
    cmp     lr, #0
#if defined(WORKAROUND_CORTEX_A9_745320)
    /* Don't use conditional loads if the HW defect exists */
    beq     101f
    ldr     r1,[lr, #3]
101:
#else
    ldrne   r1,[lr, #3]
#endif
    ldr     r2, .LinterpPunt
    mov     r0, r1                       @ set Dalvik PC
    bx      r2
    @ doesn't return

.LinterpPunt:
    .word   dvmJitToInterpPunt

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_MONITOR_ENTER
dvmCompiler_TEMPLATE_MONITOR_ENTER:
/* File: armv5te/TEMPLATE_MONITOR_ENTER.S */
    /*
     * Call out to the runtime to lock an object.  Because this thread
     * may have been suspended in THREAD_MONITOR state and the Jit's
     * translation cache subsequently cleared, we cannot return directly.
     * Instead, unconditionally transition to the interpreter to resume.
     *
     * On entry:
     *    r0 - self pointer
     *    r1 - the object (which has already been null-checked by the caller
     *    r4 - the Dalvik PC of the following instruction.
     */
    ldr     r2, .LdvmLockObject
    mov     r3, #0                       @ Record that we're not returning
    str     r3, [r0, #offThread_inJitCodeCache]
    blx     r2                           @ dvmLockObject(self, obj)
    ldr     r2, .LdvmJitToInterpNoChain
    @ Bail to interpreter - no chain [note - r4 still contains rPC]
#if defined(WITH_JIT_TUNING)
    mov     r0, #kHeavyweightMonitor
#endif
    bx      r2

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_MONITOR_ENTER_DEBUG
dvmCompiler_TEMPLATE_MONITOR_ENTER_DEBUG:
/* File: armv5te/TEMPLATE_MONITOR_ENTER_DEBUG.S */
    /*
     * To support deadlock prediction, this version of MONITOR_ENTER
     * will always call the heavyweight dvmLockObject, check for an
     * exception and then bail out to the interpreter.
     *
     * On entry:
     *    r0 - self pointer
     *    r1 - the object (which has already been null-checked by the caller
     *    r4 - the Dalvik PC of the following instruction.
     *
     */
    ldr     r2, .LdvmLockObject
    mov     r3, #0                       @ Record that we're not returning
    str     r3, [r0, #offThread_inJitCodeCache]
    blx     r2             @ dvmLockObject(self, obj)
    @ test for exception
    ldr     r1, [rSELF, #offThread_exception]
    cmp     r1, #0
    beq     1f
    ldr     r2, .LhandleException
    sub     r0, r4, #2     @ roll dPC back to this monitor instruction
    bx      r2
1:
    @ Bail to interpreter - no chain [note - r4 still contains rPC]
#if defined(WITH_JIT_TUNING)
    mov     r0, #kHeavyweightMonitor
#endif
    ldr     pc, .LdvmJitToInterpNoChain

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_PERIODIC_PROFILING
dvmCompiler_TEMPLATE_PERIODIC_PROFILING:
/* File: armv5te/TEMPLATE_PERIODIC_PROFILING.S */
    /*
     * Increment profile counter for this trace, and decrement
     * sample counter.  If sample counter goes below zero, turn
     * off profiling.
     *
     * On entry
     * (lr-11) is address of pointer to counter.  Note: the counter
     *    actually exists 10 bytes before the return target, but because
     *    we are arriving from thumb mode, lr will have its low bit set.
     */
     ldr    r0, [lr,#-11]
     ldr    r1, [rSELF, #offThread_pProfileCountdown]
     ldr    r2, [r0]                    @ get counter
     ldr    r3, [r1]                    @ get countdown timer
     add    r2, #1
     subs   r2, #1
     blt    .LTEMPLATE_PERIODIC_PROFILING_disable_profiling
     str    r2, [r0]
     str    r3, [r1]
     bx     lr

.LTEMPLATE_PERIODIC_PROFILING_disable_profiling:
     mov    r4, lr                     @ preserve lr
     ldr    r0, .LdvmJitTraceProfilingOff
     blx    r0
     bx     r4

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_RETURN_PROF
dvmCompiler_TEMPLATE_RETURN_PROF:
/* File: armv5te/TEMPLATE_RETURN_PROF.S */
#define TEMPLATE_INLINE_PROFILING
/* File: armv5te/TEMPLATE_RETURN.S */
    /*
     * Unwind a frame from the Dalvik stack for compiled OP_RETURN_XXX.
     * If the stored value in returnAddr
     * is non-zero, the caller is compiled by the JIT thus return to the
     * address in the code cache following the invoke instruction. Otherwise
     * return to the special dvmJitToInterpNoChain entry point.
     */
#if defined(TEMPLATE_INLINE_PROFILING)
    stmfd   sp!, {r0-r2,lr}             @ preserve live registers
    mov     r0, r6
    @ r0=rSELF
    ldr     ip, .LdvmFastMethodTraceExit
    blx     ip
    ldmfd   sp!, {r0-r2,lr}             @ restore live registers
#endif
    SAVEAREA_FROM_FP(r0, rFP)           @ r0<- saveArea (old)
    ldr     r10, [r0, #offStackSaveArea_prevFrame] @ r10<- saveArea->prevFrame
    ldrb    r8, [rSELF, #offThread_breakFlags] @ r8<- breakFlags
    ldr     rPC, [r0, #offStackSaveArea_savedPc] @ rPC<- saveArea->savedPc
    ldr     r2, [r10, #(offStackSaveArea_method - sizeofStackSaveArea)]
#if !defined(WITH_SELF_VERIFICATION)
    ldr     r9,  [r0, #offStackSaveArea_returnAddr] @ r9<- chaining cell ret
#else
    mov     r9, #0                      @ disable chaining
#endif
                                        @ r2<- method we're returning to
    cmp     r2, #0                      @ break frame?
#if !defined(WITH_SELF_VERIFICATION)
    beq     1f                          @ bail to interpreter
#else
    blxeq   lr                          @ punt to interpreter and compare state
#endif
    ldr     r1, .LdvmJitToInterpNoChainNoProfile @ defined in footer.S
    mov     rFP, r10                    @ publish new FP
    ldr     r10, [r2, #offMethod_clazz] @ r10<- method->clazz

    str     r2, [rSELF, #offThread_method]@ self->method = newSave->method
    ldr     r0, [r10, #offClassObject_pDvmDex] @ r0<- method->clazz->pDvmDex
    str     rFP, [rSELF, #offThread_curFrame] @ curFrame = fp
    add     rPC, rPC, #6                @ publish new rPC (advance 6 bytes)
    str     r0, [rSELF, #offThread_methodClassDex]
    cmp     r8, #0                      @ check the break flags
    movne   r9, #0                      @ clear the chaining cell address
    str     r9, [rSELF, #offThread_inJitCodeCache] @ in code cache or not
    cmp     r9, #0                      @ chaining cell exists?
    blxne   r9                          @ jump to the chaining cell
#if defined(WITH_JIT_TUNING)
    mov     r0, #kCallsiteInterpreted
#endif
    mov     pc, r1                      @ callsite is interpreted
1:
    mov     r0, #0
    str     r0, [rSELF, #offThread_inJitCodeCache] @ reset inJitCodeCache
    stmia   rSELF, {rPC, rFP}           @ SAVE_PC_FP_TO_SELF()
    ldr     r2, .LdvmMterpStdBail       @ defined in footer.S
    mov     r0, rSELF                   @ Expecting rSELF in r0
    blx     r2                          @ exit the interpreter

#undef TEMPLATE_INLINE_PROFILING

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_NO_OPT_PROF
dvmCompiler_TEMPLATE_INVOKE_METHOD_NO_OPT_PROF:
/* File: armv5te/TEMPLATE_INVOKE_METHOD_NO_OPT_PROF.S */
#define TEMPLATE_INLINE_PROFILING
/* File: armv5te/TEMPLATE_INVOKE_METHOD_NO_OPT.S */
    /*
     * For polymorphic callsites - setup the Dalvik frame and load Dalvik PC
     * into rPC then jump to dvmJitToInterpNoChain to dispatch the
     * runtime-resolved callee.
     */
    @ r0 = methodToCall, r1 = returnCell, rPC = dalvikCallsite
    ldrh    r7, [r0, #offMethod_registersSize]  @ r7<- methodToCall->regsSize
    ldrh    r2, [r0, #offMethod_outsSize]  @ r2<- methodToCall->outsSize
    ldr     r9, [rSELF, #offThread_interpStackEnd]    @ r9<- interpStackEnd
    ldrb    r8, [rSELF, #offThread_breakFlags] @ r8<- breakFlags
    add     r3, r1, #1  @ Thumb addr is odd
    SAVEAREA_FROM_FP(r1, rFP)           @ r1<- stack save area
    sub     r1, r1, r7, lsl #2          @ r1<- newFp (old savearea - regsSize)
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- stack save area
    sub     r10, r10, r2, lsl #2        @ r10<- bottom (newsave - outsSize)
    cmp     r10, r9                     @ bottom < interpStackEnd?
    bxlo    lr                          @ return to raise stack overflow excep.
    @ r1 = newFP, r0 = methodToCall, r3 = returnCell, rPC = dalvikCallsite
    ldr     r9, [r0, #offMethod_clazz]      @ r9<- method->clazz
    ldr     r10, [r0, #offMethod_accessFlags] @ r10<- methodToCall->accessFlags
    str     rPC, [rFP, #(offStackSaveArea_currentPc - sizeofStackSaveArea)]
    str     rPC, [r1, #(offStackSaveArea_savedPc - sizeofStackSaveArea)]
    ldr     rPC, [r0, #offMethod_insns]     @ rPC<- methodToCall->insns


    @ set up newSaveArea
    str     rFP, [r1, #(offStackSaveArea_prevFrame - sizeofStackSaveArea)]
    str     r3, [r1, #(offStackSaveArea_returnAddr - sizeofStackSaveArea)]
    str     r0, [r1, #(offStackSaveArea_method - sizeofStackSaveArea)]
    cmp     r8, #0                      @ breakFlags != 0
    bxne    lr                          @ bail to the interpreter
    tst     r10, #ACC_NATIVE
#if !defined(WITH_SELF_VERIFICATION)
    bne     .LinvokeNative
#else
    bxne    lr                          @ bail to the interpreter
#endif

    ldr     r10, .LdvmJitToInterpTraceSelectNoChain
    ldr     r3, [r9, #offClassObject_pDvmDex] @ r3<- method->clazz->pDvmDex

    @ Update "thread" values for the new method
    str     r0, [rSELF, #offThread_method]    @ self->method = methodToCall
    str     r3, [rSELF, #offThread_methodClassDex] @ self->methodClassDex = ...
    mov     rFP, r1                         @ fp = newFp
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = newFp
#if defined(TEMPLATE_INLINE_PROFILING)
    stmfd   sp!, {r0-r3}                    @ preserve r0-r3
    mov     r1, r6
    @ r0=methodToCall, r1=rSELF
    ldr     ip, .LdvmFastMethodTraceEnter
    blx     ip
    ldmfd   sp!, {r0-r3}                    @ restore r0-r3
#endif

    @ Start executing the callee
#if defined(WITH_JIT_TUNING)
    mov     r0, #kInlineCacheMiss
#endif
    bx      r10                         @ dvmJitToInterpTraceSelectNoChain

#undef TEMPLATE_INLINE_PROFILING

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_CHAIN_PROF
dvmCompiler_TEMPLATE_INVOKE_METHOD_CHAIN_PROF:
/* File: armv5te/TEMPLATE_INVOKE_METHOD_CHAIN_PROF.S */
#define TEMPLATE_INLINE_PROFILING
/* File: armv5te/TEMPLATE_INVOKE_METHOD_CHAIN.S */
    /*
     * For monomorphic callsite, setup the Dalvik frame and return to the
     * Thumb code through the link register to transfer control to the callee
     * method through a dedicated chaining cell.
     */
    @ r0 = methodToCall, r1 = returnCell, r2 = methodToCall->outsSize
    @ rPC = dalvikCallsite, r7 = methodToCall->registersSize
    @ methodToCall is guaranteed to be non-native
.LinvokeChainProf:
    ldr     r9, [rSELF, #offThread_interpStackEnd]    @ r9<- interpStackEnd
    ldrb    r8, [rSELF, #offThread_breakFlags]        @ r8<- breakFlags
    add     r3, r1, #1  @ Thumb addr is odd
    SAVEAREA_FROM_FP(r1, rFP)           @ r1<- stack save area
    sub     r1, r1, r7, lsl #2          @ r1<- newFp (old savearea - regsSize)
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- stack save area
    add     r12, lr, #2                 @ setup the punt-to-interp address
    sub     r10, r10, r2, lsl #2        @ r10<- bottom (newsave - outsSize)
    cmp     r10, r9                     @ bottom < interpStackEnd?
    bxlo    r12                         @ return to raise stack overflow excep.
    @ r1 = newFP, r0 = methodToCall, r3 = returnCell, rPC = dalvikCallsite
    ldr     r9, [r0, #offMethod_clazz]      @ r9<- method->clazz
    str     rPC, [rFP, #(offStackSaveArea_currentPc - sizeofStackSaveArea)]
    str     rPC, [r1, #(offStackSaveArea_savedPc - sizeofStackSaveArea)]

    @ set up newSaveArea
    str     rFP, [r1, #(offStackSaveArea_prevFrame - sizeofStackSaveArea)]
    str     r3, [r1, #(offStackSaveArea_returnAddr - sizeofStackSaveArea)]
    str     r0, [r1, #(offStackSaveArea_method - sizeofStackSaveArea)]
    cmp     r8, #0                      @ breakFlags != 0
    bxne    r12                         @ bail to the interpreter

    ldr     r3, [r9, #offClassObject_pDvmDex] @ r3<- method->clazz->pDvmDex

    @ Update "thread" values for the new method
    str     r0, [rSELF, #offThread_method]    @ self->method = methodToCall
    str     r3, [rSELF, #offThread_methodClassDex] @ self->methodClassDex = ...
    mov     rFP, r1                         @ fp = newFp
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = newFp
#if defined(TEMPLATE_INLINE_PROFILING)
    stmfd   sp!, {r0-r2,lr}             @ preserve clobbered live registers
    mov     r1, r6
    @ r0=methodToCall, r1=rSELF
    ldr     ip, .LdvmFastMethodTraceEnter
    blx     ip
    ldmfd   sp!, {r0-r2,lr}             @ restore registers
#endif

    bx      lr                              @ return to the callee-chaining cell

#undef TEMPLATE_INLINE_PROFILING

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN_PROF
dvmCompiler_TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN_PROF:
/* File: armv5te/TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN_PROF.S */
#define TEMPLATE_INLINE_PROFILING
/* File: armv5te/TEMPLATE_INVOKE_METHOD_PREDICTED_CHAIN.S */
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
     *      const ClassObject *clazz;
     *      const Method *method;
     *      u4 counter;
     *  } PredictedChainingCell;
     *
     * Upon returning to the callsite:
     *    - lr  : to branch to the chaining cell
     *    - lr+2: to punt to the interpreter
     *    - lr+4: to fully resolve the callee and may rechain.
     *            r3 <- class
     *            r9 <- counter
     */
    @ r0 = this, r1 = returnCell, r2 = predictedChainCell, rPC = dalvikCallsite
    ldr     r3, [r0, #offObject_clazz]  @ r3 <- this->class
    ldr     r8, [r2, #4]    @ r8 <- predictedChainCell->clazz
    ldr     r0, [r2, #8]    @ r0 <- predictedChainCell->method
    ldr     r9, [rSELF, #offThread_icRechainCount] @ r1 <- shared rechainCount
    cmp     r3, r8          @ predicted class == actual class?
#if defined(WITH_JIT_TUNING)
    ldr     r7, .LdvmICHitCount
#if defined(WORKAROUND_CORTEX_A9_745320)
    /* Don't use conditional loads if the HW defect exists */
    bne     101f
    ldr     r10, [r7, #0]
101:
#else
    ldreq   r10, [r7, #0]
#endif
    add     r10, r10, #1
    streq   r10, [r7, #0]
#endif
    ldreqh  r7, [r0, #offMethod_registersSize]  @ r7<- methodToCall->regsSize
    ldreqh  r2, [r0, #offMethod_outsSize]  @ r2<- methodToCall->outsSize
    beq     .LinvokeChainProf   @ predicted chain is valid
    ldr     r7, [r3, #offClassObject_vtable] @ r7 <- this->class->vtable
    cmp     r8, #0          @ initialized class or not
    moveq   r1, #0
    subne   r1, r9, #1      @ count--
    strne   r1, [rSELF, #offThread_icRechainCount]  @ write back to thread
    add     lr, lr, #4      @ return to fully-resolve landing pad
    /*
     * r1 <- count
     * r2 <- &predictedChainCell
     * r3 <- this->class
     * r4 <- dPC
     * r7 <- this->class->vtable
     */
    bx      lr

#undef TEMPLATE_INLINE_PROFILING

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INVOKE_METHOD_NATIVE_PROF
dvmCompiler_TEMPLATE_INVOKE_METHOD_NATIVE_PROF:
/* File: armv5te/TEMPLATE_INVOKE_METHOD_NATIVE_PROF.S */
#define TEMPLATE_INLINE_PROFILING
/* File: armv5te/TEMPLATE_INVOKE_METHOD_NATIVE.S */
    @ r0 = methodToCall, r1 = returnCell, rPC = dalvikCallsite
    @ r7 = methodToCall->registersSize
    ldr     r9, [rSELF, #offThread_interpStackEnd]    @ r9<- interpStackEnd
    ldrb    r8, [rSELF, #offThread_breakFlags]        @ r8<- breakFlags
    add     r3, r1, #1  @ Thumb addr is odd
    SAVEAREA_FROM_FP(r1, rFP)           @ r1<- stack save area
    sub     r1, r1, r7, lsl #2          @ r1<- newFp (old savearea - regsSize)
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- stack save area
    cmp     r10, r9                     @ bottom < interpStackEnd?
    bxlo    lr                          @ return to raise stack overflow excep.
    @ r1 = newFP, r0 = methodToCall, r3 = returnCell, rPC = dalvikCallsite
    str     rPC, [rFP, #(offStackSaveArea_currentPc - sizeofStackSaveArea)]
    str     rPC, [r1, #(offStackSaveArea_savedPc - sizeofStackSaveArea)]

    @ set up newSaveArea
    str     rFP, [r1, #(offStackSaveArea_prevFrame - sizeofStackSaveArea)]
    str     r3, [r1, #(offStackSaveArea_returnAddr - sizeofStackSaveArea)]
    str     r0, [r1, #(offStackSaveArea_method - sizeofStackSaveArea)]
    cmp     r8, #0                      @ breakFlags != 0
    ldr     r8, [r0, #offMethod_nativeFunc] @ r8<- method->nativeFunc
#if !defined(WITH_SELF_VERIFICATION)
    bxne    lr                          @ bail to the interpreter
#else
    bx      lr                          @ bail to interpreter unconditionally
#endif

    @ go ahead and transfer control to the native code
    ldr     r9, [rSELF, #offThread_jniLocal_topCookie]@r9<-thread->localRef->...
    mov     r2, #0
    str     r1, [rSELF, #offThread_curFrame]   @ curFrame = newFp
    str     r2, [rSELF, #offThread_inJitCodeCache] @ not in the jit code cache
    str     r9, [r1, #(offStackSaveArea_localRefCookie - sizeofStackSaveArea)]
                                        @ newFp->localRefCookie=top
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- new stack save area

    mov     r2, r0                        @ arg2<- methodToCall
    mov     r0, r1                        @ arg0<- newFP
    add     r1, rSELF, #offThread_retval  @ arg1<- &retval
    mov     r3, rSELF                     @ arg3<- self
#if defined(TEMPLATE_INLINE_PROFILING)
    @ r2=methodToCall, r6=rSELF
    stmfd   sp!, {r2,r6}                @ to be consumed after JNI return
    stmfd   sp!, {r0-r3}                @ preserve r0-r3
    mov     r0, r2
    mov     r1, r6
    @ r0=JNIMethod, r1=rSELF
    ldr     ip, .LdvmFastMethodTraceEnter
    blx     ip
    ldmfd   sp!, {r0-r3}                @ restore r0-r3
#endif

    blx     r8                          @ off to the native code

#if defined(TEMPLATE_INLINE_PROFILING)
    ldmfd   sp!, {r0-r1}                @ restore r2 and r6
    @ r0=JNIMethod, r1=rSELF
    ldr     ip, .LdvmFastNativeMethodTraceExit
    blx     ip
#endif
    @ native return; r10=newSaveArea
    @ equivalent to dvmPopJniLocals
    ldr     r2, [r10, #offStackSaveArea_returnAddr] @ r2 = chaining cell ret
    ldr     r0, [r10, #offStackSaveArea_localRefCookie] @ r0<- saved->top
    ldr     r1, [rSELF, #offThread_exception] @ check for exception
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = fp
    cmp     r1, #0                      @ null?
    str     r0, [rSELF, #offThread_jniLocal_topCookie] @ new top <- old top
    ldr     r0, [rFP, #(offStackSaveArea_currentPc - sizeofStackSaveArea)]

    @ r0 = dalvikCallsitePC
    bne     .LhandleException           @ no, handle exception

    str     r2, [rSELF, #offThread_inJitCodeCache] @ set the mode properly
    cmp     r2, #0                      @ return chaining cell still exists?
    bxne    r2                          @ yes - go ahead

    @ continue executing the next instruction through the interpreter
    ldr     r1, .LdvmJitToInterpTraceSelectNoChain @ defined in footer.S
    add     rPC, r0, #6                 @ reconstruct new rPC (advance 6 bytes)
#if defined(WITH_JIT_TUNING)
    mov     r0, #kCallsiteInterpreted
#endif
    mov     pc, r1

#undef TEMPLATE_INLINE_PROFILING

    .size   dvmCompilerTemplateStart, .-dvmCompilerTemplateStart
/* File: armv5te/footer.S */
/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .section .data.rel.ro
    .align  2
.LinvokeNative:
    @ Prep for the native call
    @ r1 = newFP, r0 = methodToCall
    mov     r2, #0
    ldr     r9, [rSELF, #offThread_jniLocal_topCookie]@r9<-thread->localRef->...
    str     r2, [rSELF, #offThread_inJitCodeCache] @ not in jit code cache
    str     r1, [rSELF, #offThread_curFrame]   @ curFrame = newFp
    str     r9, [r1, #(offStackSaveArea_localRefCookie - sizeofStackSaveArea)]
                                        @ newFp->localRefCookie=top
    ldrh    lr, [rSELF, #offThread_subMode]
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- new stack save area

    mov     r2, r0                      @ r2<- methodToCall
    mov     r0, r1                      @ r0<- newFP
    add     r1, rSELF, #offThread_retval  @ r1<- &retval
    mov     r3, rSELF                   @ arg3<- self
    ands    lr, #kSubModeMethodTrace
    beq     121f                        @ hop if not profiling
    @ r2: methodToCall, r6: rSELF
    stmfd   sp!, {r2,r6}
    stmfd   sp!, {r0-r3}
    mov     r0, r2
    mov     r1, r6
    ldr     ip, .LdvmFastMethodTraceEnter
    blx     ip
    ldmfd   sp!, {r0-r3}

    ldr     ip, [r2, #offMethod_nativeFunc]
    blx     ip

    ldmfd   sp!, {r0-r1}
    ldr     ip, .LdvmFastNativeMethodTraceExit
    blx     ip
    b       212f
121:
    ldr     ip, [r2, #offMethod_nativeFunc]
    blx     ip
212:

    @ native return; r10=newSaveArea
    @ equivalent to dvmPopJniLocals
    ldr     r2, [r10, #offStackSaveArea_returnAddr] @ r2 = chaining cell ret
    ldr     r0, [r10, #offStackSaveArea_localRefCookie] @ r0<- saved->top
    ldr     r1, [rSELF, #offThread_exception] @ check for exception
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = fp
    cmp     r1, #0                      @ null?
    str     r0, [rSELF, #offThread_jniLocal_topCookie] @ new top <- old top
    ldr     r0, [r10, #offStackSaveArea_savedPc] @ reload rPC

    @ r0 = dalvikCallsitePC
    bne     .LhandleException           @ no, handle exception

    str     r2, [rSELF, #offThread_inJitCodeCache] @ set the new mode
    cmp     r2, #0                      @ return chaining cell still exists?
    bxne    r2                          @ yes - go ahead

    @ continue executing the next instruction through the interpreter
    ldr     r1, .LdvmJitToInterpTraceSelectNoChain @ defined in footer.S
    add     rPC, r0, #6                 @ reconstruct new rPC (advance 6 bytes)
#if defined(WITH_JIT_TUNING)
    mov     r0, #kCallsiteInterpreted
#endif
    bx      r1

/*
 * On entry:
 * r0  Faulting Dalvik PC
 */
.LhandleException:
#if defined(WITH_SELF_VERIFICATION)
    ldr     pc, .LdeadFood @ should not see this under self-verification mode
.LdeadFood:
    .word   0xdeadf00d
#endif
    mov     r2, #0
    str     r2, [rSELF, #offThread_inJitCodeCache] @ in interpreter land
    ldr     r1, .LdvmMterpCommonExceptionThrown @ PIC way of getting &func
    ldr     rIBASE, .LdvmAsmInstructionStart    @ same as above
    mov     rPC, r0                 @ reload the faulting Dalvik address
    bx      r1                  @ branch to dvmMterpCommonExceptionThrown

    .align  2
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
.LdvmJitTraceProfilingOff:
    .word   dvmJitTraceProfilingOff
#if defined(WITH_JIT_TUNING)
.LdvmICHitCount:
    .word   gDvmICHitCount
#endif
#if defined(WITH_SELF_VERIFICATION)
.LdvmSelfVerificationMemOpDecode:
    .word   dvmSelfVerificationMemOpDecode
#endif
.LdvmFastMethodTraceEnter:
    .word   dvmFastMethodTraceEnter
.LdvmFastNativeMethodTraceExit:
    .word   dvmFastNativeMethodTraceExit
.LdvmFastMethodTraceExit:
    .word   dvmFastMethodTraceExit
.L__aeabi_cdcmple:
    .word   __aeabi_cdcmple
.L__aeabi_cfcmple:
    .word   __aeabi_cfcmple

    .global dvmCompilerTemplateEnd
dvmCompilerTemplateEnd:

#endif /* WITH_JIT */

