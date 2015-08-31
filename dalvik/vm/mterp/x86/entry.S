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
    subl    $$(FRAME_SIZE-4), %esp

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
    movl        $$0, offThread_inJitCodeCache(%ecx)
    cmpl        $$0, %eax
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
    addl    $$(FRAME_SIZE-4), %ebp       # Restore %ebp at point of setjmp
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

