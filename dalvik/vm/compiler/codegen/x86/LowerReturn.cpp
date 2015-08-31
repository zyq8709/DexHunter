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


/*! \file LowerReturn.cpp
    \brief This file lowers the following bytecodes: RETURN

*/
#include "libdex/DexOpcodes.h"
#include "libdex/DexFile.h"
#include "mterp/Mterp.h"
#include "Lower.h"
#include "enc_wrapper.h"
#include "NcgHelper.h"

//4 GPRs and scratch registers used in get_self_pointer, set_glue_method and set_glue_dvmdex
//will jump to "gotoBail" if caller method is NULL or if debugger is active
//what is %edx for each case? for the latter case, it is 1
#define P_GPR_1 PhysicalReg_ECX //must be ecx
#define P_GPR_2 PhysicalReg_EBX
#define P_SCRATCH_1 PhysicalReg_EDX
#define P_OLD_FP PhysicalReg_EAX
/*!
\brief common section to return from a method

If the helper switch is on, this will generate a helper function
*/
int common_returnFromMethod() {
#if defined(ENABLE_TRACING) && !defined(TRACING_OPTION2)
    insertMapWorklist(offsetPC, mapFromBCtoNCG[offsetPC], 1); //check when helper switch is on
#endif

    scratchRegs[0] = PhysicalReg_SCRATCH_7;
    get_self_pointer(2, false);

    //update rFP to caller stack frame
    move_reg_to_reg(OpndSize_32, PhysicalReg_FP, true, 10, false);
    move_mem_to_reg(OpndSize_32, -sizeofStackSaveArea+offStackSaveArea_prevFrame, PhysicalReg_FP, true, PhysicalReg_FP, true); //update rFP
    //get caller method by accessing the stack save area
    move_mem_to_reg(OpndSize_32, -sizeofStackSaveArea+offStackSaveArea_method, PhysicalReg_FP, true, 6, false);
    compare_imm_reg(OpndSize_32, 0, 6, false);
    conditional_jump(Condition_E, "common_gotoBail_0", false);
    get_self_pointer(3, false);
    //update glue->method
    move_reg_to_mem(OpndSize_32, 6, false, offsetof(Thread, interpSave.method), 2, false);
    //get clazz of caller method
    move_mem_to_reg(OpndSize_32, offMethod_clazz, 6, false, 14, false);
    //update self->frame
    move_reg_to_mem(OpndSize_32, PhysicalReg_FP, true, offThread_curFrame, 3, false);
    //get method->clazz->pDvmDex
    move_mem_to_reg(OpndSize_32, offClassObject_pDvmDex, 14, false, 7, false);
    move_reg_to_mem(OpndSize_32, 7, false, offsetof(Thread, interpSave.methodClassDex), 2, false);

    compare_imm_mem(OpndSize_32, 0, offsetof(Thread, suspendCount), 2, false); /* suspendCount */
    move_mem_to_reg(OpndSize_32, -sizeofStackSaveArea+offStackSaveArea_returnAddr, 10, false, PhysicalReg_EBX, true);
    move_imm_to_reg(OpndSize_32, 0, 17, false);
    /* if suspendCount is not zero, clear the chaining cell address */
    conditional_move_reg_to_reg(OpndSize_32, Condition_NZ, 17, false/*src*/, PhysicalReg_EBX, true/*dst*/);
    move_mem_to_reg(OpndSize_32, -sizeofStackSaveArea+offStackSaveArea_savedPc, 10, false, PhysicalReg_EAX, true);
    //if returnAddr is not NULL, the thread is still in code cache
    move_reg_to_mem(OpndSize_32, PhysicalReg_EBX, true, offThread_inJitCodeCache, 3, false);

    insertLabel(".LreturnToInterp", true); //local label
    //move rPC by 6 (3 bytecode units for INVOKE)
    alu_binary_imm_reg(OpndSize_32, add_opc, 6, PhysicalReg_EAX, true);

    //returnAddr in %ebx, if not zero, jump to returnAddr
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EBX, true);
    conditional_jump(Condition_E, ".LcontinueToInterp", true);
#ifdef DEBUG_CALL_STACK3
    move_reg_to_reg(OpndSize_32, PhysicalReg_EBX, true, PhysicalReg_ESI, true);
    move_imm_to_reg(OpndSize_32, 0xaabb, PhysicalReg_EBX, true);
    scratchRegs[0] = PhysicalReg_EAX;
    call_debug_dumpSwitch(); //%ebx, %eax, %edx
    move_reg_to_reg(OpndSize_32, PhysicalReg_ESI, true, PhysicalReg_EBX, true);
    call_debug_dumpSwitch();
    move_reg_to_reg(OpndSize_32, PhysicalReg_ESI, true, PhysicalReg_EBX, true);
#endif
    unconditional_jump_reg(PhysicalReg_EBX, true);
    insertLabel(".LcontinueToInterp", true);
    scratchRegs[0] = PhysicalReg_SCRATCH_4;
    typedef void (*vmHelper)(int);
    vmHelper funcPtr = dvmJitToInterpNoChainNoProfile; //%eax is the input
    move_imm_to_reg(OpndSize_32, (int)funcPtr, C_SCRATCH_1, isScratchPhysical);
#if defined(WITH_JIT_TUNING)
    /* Return address not in code cache. Indicate that continuing with interpreter.
     */
    move_imm_to_mem(OpndSize_32, kCallsiteInterpreted, 0, PhysicalReg_ESP, true);
#endif
    unconditional_jump_reg(C_SCRATCH_1, isScratchPhysical);
    touchEax();
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_SCRATCH_1

//! lower bytecode RETURN_VOID

//! It seems that shared code cache does not support helper switch
int op_return_void() {
    int retval;
    retval = common_returnFromMethod();
    rPC += 1;
    return retval;
}

//! lower bytecode RETURN

//! It seems that shared code cache does not support helper switch
//! The return value is stored to glue->retval first
int op_return() {
    u2 vA = INST_AA(inst);

    get_virtual_reg(vA, OpndSize_32, 22, false);
    scratchRegs[0] = PhysicalReg_SCRATCH_1;
    set_return_value(OpndSize_32, 22, false);

    common_returnFromMethod();
    rPC += 1;
    return 0;
}

//! lower bytecode RETURN_WIDE

//! It seems that shared code cache does not support helper switch
//! The return value is stored to glue->retval first
int op_return_wide() {
    u2 vA = INST_AA(inst);
    get_virtual_reg(vA, OpndSize_64, 1, false);
    scratchRegs[0] = PhysicalReg_SCRATCH_10; scratchRegs[1] = PhysicalReg_Null;
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;
    set_return_value(OpndSize_64, 1, false);

    common_returnFromMethod();
    rPC += 1;
    return 0;
}
