/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "asm_support_arm.S"

    /*
     * Portable invocation stub.
     * On entry:
     *   r0 = method pointer
     *   r1 = argument array or NULL for no argument methods
     *   r2 = size of argument array in bytes
     *   r3 = (managed) thread pointer
     *   [sp] = JValue* result
     *   [sp + 4] = result type char
     */
ENTRY art_portable_invoke_stub
    push   {r0, r4, r5, r9, r11, lr}       @ spill regs
    .save  {r0, r4, r5, r9, r11, lr}
    .cfi_adjust_cfa_offset 24
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r4, 4
    .cfi_rel_offset r5, 8
    .cfi_rel_offset r9, 12
    .cfi_rel_offset r11, 16
    .cfi_rel_offset lr, 20
    mov    r11, sp                         @ save the stack pointer
    .cfi_def_cfa_register r11
    @.movsp r11
    mov    r9, r3                          @ move managed thread pointer into r9
    mov    r4, #SUSPEND_CHECK_INTERVAL     @ reset r4 to suspend check interval
    add    r5, r2, #16                     @ create space for method pointer in frame
    and    r5, #0xFFFFFFF0                 @ align frame size to 16 bytes
    sub    sp, r5                          @ reserve stack space for argument array
    add    r0, sp, #4                      @ pass stack pointer + method ptr as dest for memcpy
    bl     memcpy                          @ memcpy (dest, src, bytes)
    ldr    r0, [r11]                       @ restore method*
    ldr    r1, [sp, #4]                    @ copy arg value for r1
    ldr    r2, [sp, #8]                    @ copy arg value for r2
    ldr    r3, [sp, #12]                   @ copy arg value for r3
    mov    ip, #0                          @ set ip to 0
    str    ip, [sp]                        @ store NULL for method* at bottom of frame
    add    sp, #16                         @ first 4 args are not passed on stack for portable
    ldr    ip, [r0, #METHOD_CODE_OFFSET]   @ get pointer to the code
    blx    ip                              @ call the method
    mov    sp, r11                         @ restore the stack pointer
    ldr    ip, [sp, #24]                   @ load the result pointer
    strd   r0, [ip]                        @ store r0/r1 into result pointer
    pop    {r0, r4, r5, r9, r11, lr}       @ restore spill regs
    .cfi_adjust_cfa_offset -24
    bx     lr
END art_portable_invoke_stub

    .extern artPortableProxyInvokeHandler
ENTRY art_portable_proxy_invoke_handler
    @ Fake callee save ref and args frame set up, note portable doesn't use callee save frames.
    @ TODO: just save the registers that are needed in artPortableProxyInvokeHandler.
    push {r1-r3, r5-r8, r10-r11, lr}  @ 10 words of callee saves
    .save {r1-r3, r5-r8, r10-r11, lr}
    .cfi_adjust_cfa_offset 40
    .cfi_rel_offset r1, 0
    .cfi_rel_offset r2, 4
    .cfi_rel_offset r3, 8
    .cfi_rel_offset r5, 12
    .cfi_rel_offset r6, 16
    .cfi_rel_offset r7, 20
    .cfi_rel_offset r8, 24
    .cfi_rel_offset r10, 28
    .cfi_rel_offset r11, 32
    .cfi_rel_offset lr, 36
    sub sp, #8                        @ 2 words of space, bottom word will hold Method*
    .pad #8
    .cfi_adjust_cfa_offset 8
    @ Begin argument set up.
    str     r0, [sp, #0]           @ place proxy method at bottom of frame
    mov     r2, r9                 @ pass Thread::Current
    mov     r3, sp                 @ pass SP
    blx     artPortableProxyInvokeHandler  @ (Method* proxy method, receiver, Thread*, SP)
    ldr     r12, [r9, #THREAD_EXCEPTION_OFFSET]  @ load Thread::Current()->exception_
    ldr     lr,  [sp, #44]         @ restore lr
    add     sp,  #48               @ pop frame
    .cfi_adjust_cfa_offset -48
    bx      lr                     @ return
END art_portable_proxy_invoke_handler

    .extern artPortableResolutionTrampoline
ENTRY art_portable_resolution_trampoline
    @ Fake callee save ref and args frame set up, note portable doesn't use callee save frames.
    @ TODO: just save the registers that are needed in artPortableResolutionTrampoline.
    push {r1-r3, r5-r8, r10-r11, lr}  @ 10 words of callee saves
    .save {r1-r3, r5-r8, r10-r11, lr}
    .cfi_adjust_cfa_offset 40
    .cfi_rel_offset r1, 0
    .cfi_rel_offset r2, 4
    .cfi_rel_offset r3, 8
    .cfi_rel_offset r5, 12
    .cfi_rel_offset r6, 16
    .cfi_rel_offset r7, 20
    .cfi_rel_offset r8, 24
    .cfi_rel_offset r10, 28
    .cfi_rel_offset r11, 32
    .cfi_rel_offset lr, 36
    sub sp, #8                     @ 2 words of space, bottom word will hold Method*
    .pad #8
    .cfi_adjust_cfa_offset 8
    mov     r2, r9                 @ pass Thread::Current
    mov     r3, sp                 @ pass SP
    blx     artPortableResolutionTrampoline  @ (Method* called, receiver, Thread*, SP)
    cmp     r0, #0                 @ is code pointer null?
    beq     1f                     @ goto exception
    mov     r12, r0
    ldr  r0, [sp, #0]              @ load resolved method in r0
    ldr  r1, [sp, #8]              @ restore non-callee save r1
    ldrd r2, [sp, #12]             @ restore non-callee saves r2-r3
    ldr  lr, [sp, #44]             @ restore lr
    add  sp, #48                   @ rewind sp
    .cfi_adjust_cfa_offset -48
    bx      r12                    @ tail-call into actual code
1:
    ldr  r1, [sp, #8]          @ restore non-callee save r1
    ldrd r2, [sp, #12]         @ restore non-callee saves r2-r3
    ldr  lr, [sp, #44]         @ restore lr
    add  sp, #48               @ rewind sp
    .cfi_adjust_cfa_offset -48
    bx lr
END art_portable_resolution_trampoline

    .extern artPortableToInterpreterBridge
ENTRY art_portable_to_interpreter_bridge
    @ Fake callee save ref and args frame set up, note portable doesn't use callee save frames.
    @ TODO: just save the registers that are needed in artPortableToInterpreterBridge.
    push {r1-r3, r5-r8, r10-r11, lr}  @ 10 words of callee saves
    .save {r1-r3, r5-r8, r10-r11, lr}
    .cfi_adjust_cfa_offset 40
    .cfi_rel_offset r1, 0
    .cfi_rel_offset r2, 4
    .cfi_rel_offset r3, 8
    .cfi_rel_offset r5, 12
    .cfi_rel_offset r6, 16
    .cfi_rel_offset r7, 20
    .cfi_rel_offset r8, 24
    .cfi_rel_offset r10, 28
    .cfi_rel_offset r11, 32
    .cfi_rel_offset lr, 36
    sub sp, #8                     @ 2 words of space, bottom word will hold Method*
    .pad #8
    .cfi_adjust_cfa_offset 8
    mov     r1, r9                 @ pass Thread::Current
    mov     r2, sp                 @ pass SP
    blx     artPortableToInterpreterBridge    @ (Method* method, Thread*, SP)
    ldr     lr,  [sp, #44]         @ restore lr
    add     sp,  #48               @ pop frame
    .cfi_adjust_cfa_offset -48
    bx      lr                     @ return
END art_portable_to_interpreter_bridge
