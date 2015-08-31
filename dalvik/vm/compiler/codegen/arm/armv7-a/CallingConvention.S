/*
 * Copyright (C) 2009 The Android Open Source Project
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
 * Save & restore for callee-save FP registers.
 * On entry:
 *    r0 : pointer to save area of JIT_CALLEE_SAVE_WORD_SIZE
 */
    .text
    .align 2
    .global dvmJitCalleeSave
    .type dvmJitCalleeSave, %function
dvmJitCalleeSave:
    vstmia r0, {d8-d15}
    bx     lr

    .global dvmJitCalleeRestore
    .type dvmJitCalleeRestore, %function
dvmJitCalleeRestore:
    vldmia r0, {d8-d15}
    bx     lr
