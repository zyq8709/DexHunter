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


/*! \file LowerMove.cpp
    \brief This file lowers the following bytecodes: MOVE_XXX
*/
#include "libdex/DexOpcodes.h"
#include "libdex/DexFile.h"
#include "Lower.h"
#include "enc_wrapper.h"

#define P_GPR_1 PhysicalReg_EBX
//! lower bytecode MOVE

//!
int op_move() {
    u2 vA, vB;
    vA = INST_A(inst);
    vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_32, 1, false/*isPhysical*/);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    rPC += 1;
    return 2;
}
//! lower bytecode MOVE_FROM16

//!
int op_move_from16() {
    u2 vA, vB;
    vA = INST_AA(inst);
    vB = FETCH(1);
    get_virtual_reg(vB, OpndSize_32, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    rPC += 2;
    return 2;
}
//! lower bytecode MOVE_16

//!
int op_move_16() {
    u2 vA, vB;
    vA = FETCH(1);
    vB = FETCH(2);
    get_virtual_reg(vB, OpndSize_32, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    rPC += 3;
    return 2;
}
#undef P_GPR_1
//! lower bytecode MOVE_WIDE

//!
int op_move_wide() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_64, 1, false);
    set_virtual_reg(vA, OpndSize_64, 1, false);
    rPC += 1;
    return 2;
}
//! lower bytecode MOVE_WIDE_FROM16

//!
int op_move_wide_from16() {
    u2 vA = INST_AA(inst);
    u2 vB = FETCH(1);
    get_virtual_reg(vB, OpndSize_64, 1, false);
    set_virtual_reg(vA, OpndSize_64, 1, false);
    rPC += 2;
    return 2;
}
//! lower bytecode MOVE_WIDE_16

//!
int op_move_wide_16() {
    u2 vA = FETCH(1);
    u2 vB = FETCH(2);
    get_virtual_reg(vB, OpndSize_64, 1, false);
    set_virtual_reg(vA, OpndSize_64, 1, false);
    rPC += 3;
    return 2;
}
//! lower bytecode MOVE_RESULT.

//! the return value from bytecode INVOKE is stored in the glue structure
int op_move_result() {
#ifdef WITH_JIT_INLINING
    /* An inlined move result is effectively no-op */
    if (traceCurrentMIR->OptimizationFlags & MIR_INLINED)
        return 0;
#endif
    u2 vA = INST_AA(inst);
    scratchRegs[0] = PhysicalReg_SCRATCH_1;
    get_return_value(OpndSize_32, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    rPC += 1;
    return 0;
}
//! lower bytecode MOVE_RESULT_WIDE.

//! the return value from bytecode INVOKE is stored in the glue structure
int op_move_result_wide() {
#ifdef WITH_JIT_INLINING
    /* An inlined move result is effectively no-op */
    if (traceCurrentMIR->OptimizationFlags & MIR_INLINED)
        return 0;
#endif
    u2 vA = INST_AA(inst);
    scratchRegs[0] = PhysicalReg_SCRATCH_1;
    get_return_value(OpndSize_64, 1, false);
    set_virtual_reg(vA, OpndSize_64, 1, false);
    rPC += 1;
    return 0;
}

#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
//!lower bytecode MOVE_RESULT_EXCEPTION

//!update a virtual register with exception from glue structure;
//!clear the exception from glue structure
int op_move_exception() {
    u2 vA = INST_AA(inst);
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;
    scratchRegs[0] = PhysicalReg_SCRATCH_1; scratchRegs[1] = PhysicalReg_Null;
    get_self_pointer(2, false);
    move_mem_to_reg(OpndSize_32, offThread_exception, 2, false, 3, false);
    move_imm_to_mem(OpndSize_32, 0, offThread_exception, 2, false);
    set_virtual_reg(vA, OpndSize_32, 3, false);
    rPC += 1;
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2

