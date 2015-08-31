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
 *    a0 : pointer to save area of JIT_CALLEE_SAVE_WORD_SIZE
 */
    .text
    .align 2
    .global dvmJitCalleeSave
    .type dvmJitCalleeSave, %function
dvmJitCalleeSave:
#ifdef __mips_hard_float
    /* For performance reasons, we are not using any "callee saved" */
    /* fp registers, thus no need to save them.                     */
#endif
    jr $31

    .global dvmJitCalleeRestore
    .type dvmJitCalleeRestore, %function
dvmJitCalleeRestore:
#ifdef __mips_hard_float
    /* For performance reasons, we are not using any "callee saved" */
    /* fp registers, thus no need to restore them.                  */
#endif
    jr $31
