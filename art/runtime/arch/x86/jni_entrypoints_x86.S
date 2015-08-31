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

#include "asm_support_x86.S"

    /*
     * Portable resolution trampoline.
     */
DEFINE_FUNCTION art_jni_dlsym_lookup_stub
    subl LITERAL(8), %esp         // align stack
    .cfi_adjust_cfa_offset 8
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    call SYMBOL(artFindNativeMethod)  // (Thread*)
    addl LITERAL(12), %esp        // restore the stack
    .cfi_adjust_cfa_offset -12
    cmpl LITERAL(0), %eax         // check if returned method code is null
    je no_native_code_found       // if null, jump to return to handle
    jmp *%eax                     // otherwise, tail call to intended method
no_native_code_found:
    ret
END_FUNCTION art_jni_dlsym_lookup_stub
