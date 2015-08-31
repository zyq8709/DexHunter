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

#include "asm_support_mips.S"

    .set noreorder
    .balign 4

    .extern artPortableProxyInvokeHandler
ENTRY art_portable_proxy_invoke_handler
    GENERATE_GLOBAL_POINTER
    # Fake callee save ref and args frame set up, note portable doesn't use callee save frames.
    # TODO: just save the registers that are needed in artPortableProxyInvokeHandler.
    addiu  $sp, $sp, -64
    .cfi_adjust_cfa_offset 64
    sw     $ra, 60($sp)
    .cfi_rel_offset 31, 60
    sw     $s8, 56($sp)
    .cfi_rel_offset 30, 56
    sw     $gp, 52($sp)
    .cfi_rel_offset 28, 52
    sw     $s7, 48($sp)
    .cfi_rel_offset 23, 48
    sw     $s6, 44($sp)
    .cfi_rel_offset 22, 44
    sw     $s5, 40($sp)
    .cfi_rel_offset 21, 40
    sw     $s4, 36($sp)
    .cfi_rel_offset 20, 36
    sw     $s3, 32($sp)
    .cfi_rel_offset 19, 32
    sw     $s2, 28($sp)
    .cfi_rel_offset 18, 28
    sw     $a3, 12($sp)
    .cfi_rel_offset 7, 12
    sw     $a2, 8($sp)
    .cfi_rel_offset 6, 8
    sw     $a1, 4($sp)
    .cfi_rel_offset 5, 4
    # Begin argument set up.
    sw      $a0, 0($sp)            # place proxy method at bottom of frame
    move    $a2, rSELF             # pass Thread::Current
    jal     artPortableProxyInvokeHandler  # (Method* proxy method, receiver, Thread*, SP)
    move    $a3, $sp               # pass $sp
    lw      $ra, 60($sp)           # restore $ra
    jr      $ra
    addiu   $sp, $sp, 64           # pop frame
    .cfi_adjust_cfa_offset -64
END art_portable_proxy_invoke_handler

UNIMPLEMENTED art_portable_resolution_trampoline
UNIMPLEMENTED art_portable_to_interpreter_bridge
