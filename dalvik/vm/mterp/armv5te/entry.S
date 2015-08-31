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

