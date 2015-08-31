
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

#define ASSIST_DEBUGGER 1

    .text
    .align 2
    .global dvmMterpStdRun
    .ent dvmMterpStdRun
    .frame sp, STACK_SIZE, ra
/*
 * On entry:
 *  r0  Thread* self
 *
 * The return comes via a call to dvmMterpStdBail().
 */

dvmMterpStdRun:
    .set noreorder
    .cpload t9
    .set reorder
/* Save to the stack. Frame size = STACK_SIZE */
    STACK_STORE_FULL()
/* This directive will make sure all subsequent jal restore gp at a known offset */
    .cprestore STACK_OFFSET_GP

    addu      fp, sp, STACK_SIZE           #  Move Frame Pointer to the base of frame
    /* save stack pointer, add magic word for debuggerd */
    sw        sp, offThread_bailPtr(a0)      # Save SP

    /* set up "named" registers, figure out entry point */
    move      rSELF, a0                    #  set rSELF
    LOAD_PC_FROM_SELF()
    LOAD_FP_FROM_SELF()
    lw        rIBASE, offThread_curHandlerTable(rSELF)

#if defined(WITH_JIT)
.LentryInstr:
    /* Entry is always a possible trace start */
    lw        a0, offThread_pJitProfTable(rSELF)
    FETCH_INST()                           #  load rINST from rPC
    sw        zero, offThread_inJitCodeCache(rSELF)
#if !defined(WITH_SELF_VERIFICATION)
    bnez      a0, common_updateProfile     # profiling is enabled
#else
    lw       a2, offThread_shadowSpace(rSELF) # to find out the jit exit state
    beqz     a0, 1f                        # profiling is disabled
    lw       a3, offShadowSpace_jitExitState(a2) # jit exit state
    li	     t0, kSVSTraceSelect
    bne      a3, t0, 2f
    li       a2, kJitTSelectRequestHot     # ask for trace selection
    b        common_selectTrace            # go build the trace
2:
    li       a4, kSVSNoProfile
    beq      a3, a4, 1f                    # don't profile the next instruction?
    b        common_updateProfile          # collect profiles
#endif
1:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
#else
    /* start executing the instruction at rPC */
    FETCH_INST()                           #  load rINST from rPC
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
#endif

.Lbad_arg:
    la        a0, .LstrBadEntryPoint
    #a1 holds value of entryPoint
    JAL(printf)
    JAL(dvmAbort)

    .end dvmMterpStdRun

    .global dvmMterpStdBail
    .ent dvmMterpStdBail

/* Restore the stack pointer and all the registers stored at sp from the save
 * point established  on entry. Return to whoever called dvmMterpStdRun.
 *
 * On entry:
 *   a0    Thread* self
 */
dvmMterpStdBail:
    lw        sp, offThread_bailPtr(a0)      #  Restore sp
    STACK_LOAD_FULL()
    jr        ra

    .end dvmMterpStdBail
