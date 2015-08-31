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


/*! \file LowerAlu.cpp
    \brief This file lowers ALU bytecodes.
*/
#include "libdex/DexOpcodes.h"
#include "libdex/DexFile.h"
#include "Lower.h"
#include "NcgAot.h"
#include "enc_wrapper.h"

/////////////////////////////////////////////
#define P_GPR_1 PhysicalReg_EBX
//! lower bytecode NEG_INT

//!
int op_neg_int() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_32, 1, false);
    alu_unary_reg(OpndSize_32, neg_opc, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    rPC += 1;
    return 0;
}
//! lower bytecode NOT_INT

//!
int op_not_int() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_32, 1, false);
    alu_unary_reg(OpndSize_32, not_opc, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    rPC += 1;
    return 0;
}
#undef P_GPR_1
//! lower bytecode NEG_LONG

//! This implementation uses XMM registers
int op_neg_long() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_64, 1, false);
    alu_binary_reg_reg(OpndSize_64, xor_opc, 2, false, 2, false);
    alu_binary_reg_reg(OpndSize_64, sub_opc, 1, false, 2, false);
    set_virtual_reg(vA, OpndSize_64, 2, false);
    rPC += 1;
    return 0;
}
//! lower bytecode NOT_LONG

//! This implementation uses XMM registers
int op_not_long() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_64, 1, false);
    load_global_data_API("64bits", OpndSize_64, 2, false);
    alu_binary_reg_reg(OpndSize_64, andn_opc, 2, false, 1, false);
    set_virtual_reg(vA, OpndSize_64, 1, false);
    rPC += 1;
    return 0;
}
#define P_GPR_1 PhysicalReg_EBX
//! lower bytecode NEG_FLOAT

//! This implementation uses GPR
int op_neg_float() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_32, 1, false);
    alu_binary_imm_reg(OpndSize_32, add_opc, 0x80000000, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    rPC += 1;
    return 0;
}
#undef P_GPR_1

//! lower bytecode NEG_DOUBLE

//! This implementation uses XMM registers
int op_neg_double() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_64, 1, false);
    load_global_data_API("doubNeg", OpndSize_64, 2, false);
    alu_binary_reg_reg(OpndSize_64, xor_opc, 1, false, 2, false);
    set_virtual_reg(vA, OpndSize_64, 2, false);
    rPC += 1;
    return 0;
}

//! lower bytecode INT_TO_LONG

//! It uses native instruction cdq
int op_int_to_long() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_32, PhysicalReg_EAX, true);
    convert_integer(OpndSize_32, OpndSize_64);
    set_virtual_reg(vA, OpndSize_32, PhysicalReg_EAX, true);
    set_virtual_reg(vA+1, OpndSize_32, PhysicalReg_EDX, true);
    rPC += 1;
    return 0;
}
//! lower bytecode INT_TO_FLOAT

//! This implementation uses FP stack
int op_int_to_float() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    load_int_fp_stack_VR(OpndSize_32, vB); //fildl
    store_fp_stack_VR(true, OpndSize_32, vA); //fstps
    rPC += 1;
    return 0;
}
//! lower bytecode INT_TO_DOUBLE

//! This implementation uses FP stack
int op_int_to_double() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    load_int_fp_stack_VR(OpndSize_32, vB); //fildl
    store_fp_stack_VR(true, OpndSize_64, vA); //fstpl
    rPC += 1;
    return 0;
}
//! lower bytecode LONG_TO_FLOAT

//! This implementation uses FP stack
int op_long_to_float() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    load_int_fp_stack_VR(OpndSize_64, vB); //fildll
    store_fp_stack_VR(true, OpndSize_32, vA); //fstps
    rPC += 1;
    return 0;
}
//! lower bytecode LONG_TO_DOUBLE

//! This implementation uses FP stack
int op_long_to_double() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    load_int_fp_stack_VR(OpndSize_64, vB); //fildll
    store_fp_stack_VR(true, OpndSize_64, vA); //fstpl
    rPC += 1;
    return 0;
}
//! lower bytecode FLOAT_TO_DOUBLE

//! This implementation uses FP stack
int op_float_to_double() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    load_fp_stack_VR(OpndSize_32, vB); //flds
    store_fp_stack_VR(true, OpndSize_64, vA); //fstpl
    rPC += 1;
    return 0;
}
//! lower bytecode DOUBLE_TO_FLOAT

//! This implementation uses FP stack
int op_double_to_float() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    load_fp_stack_VR(OpndSize_64, vB); //fldl
    store_fp_stack_VR(true, OpndSize_32, vA); //fstps
    rPC += 1;
    return 0;
}
#define P_GPR_1 PhysicalReg_EBX
//! lower bytecode LONG_TO_INT

//! This implementation uses GPR
int op_long_to_int() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_32, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    rPC += 1;
    return 0;
}
#undef P_GPR_1

//! common code to convert a float or double to integer

//! It uses FP stack
int common_fp_to_int(bool isDouble, u2 vA, u2 vB) {
    if(isDouble) {
        load_fp_stack_VR(OpndSize_64, vB); //fldl
    }
    else {
        load_fp_stack_VR(OpndSize_32, vB); //flds
    }

    load_fp_stack_global_data_API("intMax", OpndSize_32);
    load_fp_stack_global_data_API("intMin", OpndSize_32);

    //ST(0) ST(1) ST(2) --> LintMin LintMax value
    compare_fp_stack(true, 2, false/*isDouble*/); //ST(2)
    //ST(0) ST(1) --> LintMax value
    conditional_jump(Condition_AE, ".float_to_int_negInf", true);
    rememberState(1);
    compare_fp_stack(true, 1, false/*isDouble*/); //ST(1)
    //ST(0) --> value
    rememberState(2);
    conditional_jump(Condition_C, ".float_to_int_nanInf", true);
    //fnstcw, orw, fldcw, xorw
    load_effective_addr(-2, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    store_fpu_cw(false/*checkException*/, 0, PhysicalReg_ESP, true);
    alu_binary_imm_mem(OpndSize_16, or_opc, 0xc00, 0, PhysicalReg_ESP, true);
    load_fpu_cw(0, PhysicalReg_ESP, true);
    alu_binary_imm_mem(OpndSize_16, xor_opc, 0xc00, 0, PhysicalReg_ESP, true);
    store_int_fp_stack_VR(true/*pop*/, OpndSize_32, vA); //fistpl
    //fldcw
    load_fpu_cw(0, PhysicalReg_ESP, true);
    load_effective_addr(2, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    rememberState(3);
    unconditional_jump(".float_to_int_okay", true);
    insertLabel(".float_to_int_nanInf", true);
    conditional_jump(Condition_NP, ".float_to_int_posInf", true);
    //fstps CHECK
    goToState(2);
    store_fp_stack_VR(true, OpndSize_32, vA);
    set_VR_to_imm(vA, OpndSize_32, 0);
    transferToState(3);
    unconditional_jump(".float_to_int_okay", true);
    insertLabel(".float_to_int_posInf", true);
    //fstps CHECK
    goToState(2);
    store_fp_stack_VR(true, OpndSize_32, vA);
    set_VR_to_imm(vA, OpndSize_32, 0x7fffffff);
    transferToState(3);
    unconditional_jump(".float_to_int_okay", true);
    insertLabel(".float_to_int_negInf", true);
    goToState(1);
    //fstps CHECK
    store_fp_stack_VR(true, OpndSize_32, vA);
    store_fp_stack_VR(true, OpndSize_32, vA);
    set_VR_to_imm(vA, OpndSize_32, 0x80000000);
    transferToState(3);
    insertLabel(".float_to_int_okay", true);
    return 0;
}
//! lower bytecode FLOAT_TO_INT by calling common_fp_to_int

//!
int op_float_to_int() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    int retval = common_fp_to_int(false, vA, vB);
    rPC += 1;
    return retval;
}
//! lower bytecode DOUBLE_TO_INT by calling common_fp_to_int

//!
int op_double_to_int() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    int retval = common_fp_to_int(true, vA, vB);
    rPC += 1;
    return retval;
}

//! common code to convert float or double to long

//! It uses FP stack
int common_fp_to_long(bool isDouble, u2 vA, u2 vB) {
    if(isDouble) {
        load_fp_stack_VR(OpndSize_64, vB); //fldl
    }
    else {
        load_fp_stack_VR(OpndSize_32, vB); //flds
    }

    //Check if it is the special Negative Infinity value
    load_fp_stack_global_data_API("valueNegInfLong", OpndSize_64);
    //Stack status: ST(0) ST(1) --> LlongMin value
    compare_fp_stack(true, 1, false/*isDouble*/); // Pops ST(1)
    conditional_jump(Condition_AE, ".float_to_long_negInf", true);
    rememberState(1);

    //Check if it is the special Positive Infinity value
    load_fp_stack_global_data_API("valuePosInfLong", OpndSize_64);
    //Stack status: ST(0) ST(1) --> LlongMax value
    compare_fp_stack(true, 1, false/*isDouble*/); // Pops ST(1)
    rememberState(2);
    conditional_jump(Condition_C, ".float_to_long_nanInf", true);

    //Normal Case
    //We want to truncate to 0 for conversion. That will be rounding mode 0x11
    load_effective_addr(-2, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    store_fpu_cw(false/*checkException*/, 0, PhysicalReg_ESP, true);
    //Change control word to rounding mode 11:
    alu_binary_imm_mem(OpndSize_16, or_opc, 0xc00, 0, PhysicalReg_ESP, true);
    //Load the control word
    load_fpu_cw(0, PhysicalReg_ESP, true);
    //Reset the control word
    alu_binary_imm_mem(OpndSize_16, xor_opc, 0xc00, 0, PhysicalReg_ESP, true);
    //Perform the actual conversion
    store_int_fp_stack_VR(true/*pop*/, OpndSize_64, vA); //fistpll
    // Restore the original control word
    load_fpu_cw(0, PhysicalReg_ESP, true);
    load_effective_addr(2, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    rememberState(3);
    /* NOTE: We do not need to pop out the original value we pushed
     * since load_fpu_cw above already clears the stack for
     * normal values.
     */
    unconditional_jump(".float_to_long_okay", true);

    //We can be here for positive infinity or NaN. Check parity bit
    insertLabel(".float_to_long_nanInf", true);
    conditional_jump(Condition_NP, ".float_to_long_posInf", true);
    goToState(2);
    //Save corresponding Long NaN value
    load_global_data_API("valueNanLong", OpndSize_64, 1, false);
    set_virtual_reg(vA, OpndSize_64, 1, false);
    transferToState(3);
    //Pop out the original value we pushed
    compare_fp_stack(true, 0, false/*isDouble*/); //ST(0)
    unconditional_jump(".float_to_long_okay", true);

    insertLabel(".float_to_long_posInf", true);
    goToState(2);
    //Save corresponding Long Positive Infinity value
    load_global_data_API("valuePosInfLong", OpndSize_64, 2, false);
    set_virtual_reg(vA, OpndSize_64, 2, false);
    transferToState(3);
    //Pop out the original value we pushed
    compare_fp_stack(true, 0, false/*isDouble*/); //ST(0)
    unconditional_jump(".float_to_long_okay", true);

    insertLabel(".float_to_long_negInf", true);
    //fstpl
    goToState(1);
    //Load corresponding Long Negative Infinity value
    load_global_data_API("valueNegInfLong", OpndSize_64, 3, false);
    set_virtual_reg(vA, OpndSize_64, 3, false);
    transferToState(3);
    //Pop out the original value we pushed
    compare_fp_stack(true, 0, false/*isDouble*/); //ST(0)

    insertLabel(".float_to_long_okay", true);
    return 0;
}
//! lower bytecode FLOAT_TO_LONG by calling common_fp_to_long

//!
int op_float_to_long() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    int retval = common_fp_to_long(false, vA, vB);
    rPC += 1;
    return retval;
}
//! lower bytecode DOUBLE_TO_LONG by calling common_fp_to_long

//!
int op_double_to_long() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    int retval = common_fp_to_long(true, vA, vB);
    rPC += 1;
    return retval;
}
#define P_GPR_1 PhysicalReg_EBX
//! lower bytecode INT_TO_BYTE

//! It uses GPR
int op_int_to_byte() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_32, 1, false);
    alu_binary_imm_reg(OpndSize_32, sal_opc, 24, 1, false);
    alu_binary_imm_reg(OpndSize_32, sar_opc, 24, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    rPC += 1;
    return 0;
}
//! lower bytecode INT_TO_CHAR

//! It uses GPR
int op_int_to_char() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_32, 1, false);
    alu_binary_imm_reg(OpndSize_32, sal_opc, 16, 1, false);
    alu_binary_imm_reg(OpndSize_32, shr_opc, 16, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    rPC += 1;
    return 0;
}
//! lower bytecode INT_TO_SHORT

//! It uses GPR
int op_int_to_short() {
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst);
    get_virtual_reg(vB, OpndSize_32, 1, false);
    alu_binary_imm_reg(OpndSize_32, sal_opc, 16, 1, false);
    alu_binary_imm_reg(OpndSize_32, sar_opc, 16, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    rPC += 1;
    return 0;
}
//! common code to handle integer ALU ops

//! It uses GPR
int common_alu_int(ALU_Opcode opc, u2 vA, u2 v1, u2 v2) { //except div and rem
    get_virtual_reg(v1, OpndSize_32, 1, false);
    //in encoder, reg is first operand, which is the destination
    //gpr_1 op v2(rFP) --> gpr_1
    //shift only works with reg cl, v2 should be in %ecx
    alu_binary_VR_reg(OpndSize_32, opc, v2, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    return 0;
}
#undef P_GPR_1
#define P_GPR_1 PhysicalReg_EBX
//! common code to handle integer shift ops

//! It uses GPR
int common_shift_int(ALU_Opcode opc, u2 vA, u2 v1, u2 v2) {
    get_virtual_reg(v2, OpndSize_32, PhysicalReg_ECX, true);
    get_virtual_reg(v1, OpndSize_32, 1, false);
    //in encoder, reg2 is first operand, which is the destination
    //gpr_1 op v2(rFP) --> gpr_1
    //shift only works with reg cl, v2 should be in %ecx
    alu_binary_reg_reg(OpndSize_32, opc, PhysicalReg_ECX, true, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    return 0;
}
#undef p_GPR_1
//! lower bytecode ADD_INT by calling common_alu_int

//!
int op_add_int() {
    u2 vA, v1, v2;
    vA = INST_AA(inst);
    v1 = *((u1*)rPC + 2);
    v2 = *((u1*)rPC + 3);
    int retval = common_alu_int(add_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode SUB_INT by calling common_alu_int

//!
int op_sub_int() {
    u2 vA, v1, v2;
    vA = INST_AA(inst);
    v1 = *((u1*)rPC + 2);
    v2 = *((u1*)rPC + 3);
    int retval = common_alu_int(sub_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode MUL_INT by calling common_alu_int

//!
int op_mul_int() {
    u2 vA, v1, v2;
    vA = INST_AA(inst);
    v1 = *((u1*)rPC + 2);
    v2 = *((u1*)rPC + 3);
    int retval = common_alu_int(imul_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode AND_INT by calling common_alu_int

//!
int op_and_int() {
    u2 vA, v1, v2;
    vA = INST_AA(inst);
    v1 = *((u1*)rPC + 2);
    v2 = *((u1*)rPC + 3);
    int retval = common_alu_int(and_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode OR_INT by calling common_alu_int

//!
int op_or_int() {
    u2 vA, v1, v2;
    vA = INST_AA(inst);
    v1 = *((u1*)rPC + 2);
    v2 = *((u1*)rPC + 3);
    int retval = common_alu_int(or_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode XOR_INT by calling common_alu_int

//!
int op_xor_int() {
    u2 vA, v1, v2;
    vA = INST_AA(inst);
    v1 = *((u1*)rPC + 2);
    v2 = *((u1*)rPC + 3);
    int retval = common_alu_int(xor_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode SHL_INT by calling common_shift_int

//!
int op_shl_int() {
    u2 vA, v1, v2;
    vA = INST_AA(inst);
    v1 = *((u1*)rPC + 2);
    v2 = *((u1*)rPC + 3);
    int retval = common_shift_int(shl_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode SHR_INT by calling common_shift_int

//!
int op_shr_int() {
    u2 vA, v1, v2;
    vA = INST_AA(inst);
    v1 = *((u1*)rPC + 2);
    v2 = *((u1*)rPC + 3);
    int retval = common_shift_int(sar_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode USHR_INT by calling common_shift_int

//!
int op_ushr_int() {
    u2 vA, v1, v2;
    vA = INST_AA(inst);
    v1 = *((u1*)rPC + 2);
    v2 = *((u1*)rPC + 3);
    int retval = common_shift_int(shr_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode ADD_INT_2ADDR by calling common_alu_int

//!
int op_add_int_2addr() {
    u2 vA, v1, v2;
    vA = INST_A(inst);
    v1 = vA;
    v2 = INST_B(inst);
    int retval = common_alu_int(add_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode SUB_INT_2ADDR by calling common_alu_int

//!
int op_sub_int_2addr() {
    u2 vA, v1, v2;
    vA = INST_A(inst);
    v1 = vA;
    v2 = INST_B(inst);
    int retval = common_alu_int(sub_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode MUL_INT_2ADDR by calling common_alu_int

//!
int op_mul_int_2addr() {
    u2 vA, v1, v2;
    vA = INST_A(inst);
    v1 = vA;
    v2 = INST_B(inst);
    int retval = common_alu_int(imul_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode AND_INT_2ADDR by calling common_alu_int

//!
int op_and_int_2addr() {
    u2 vA, v1, v2;
    vA = INST_A(inst);
    v1 = vA;
    v2 = INST_B(inst);
    int retval = common_alu_int(and_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode OR_INT_2ADDR by calling common_alu_int

//!
int op_or_int_2addr() {
    u2 vA, v1, v2;
    vA = INST_A(inst);
    v1 = vA;
    v2 = INST_B(inst);
    int retval = common_alu_int(or_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode XOR_INT_2ADDR by calling common_alu_int

//!
int op_xor_int_2addr() {
    u2 vA, v1, v2;
    vA = INST_A(inst);
    v1 = vA;
    v2 = INST_B(inst);
    int retval = common_alu_int(xor_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode SHL_INT_2ADDR by calling common_shift_int

//!
int op_shl_int_2addr() {
    u2 vA, v1, v2;
    vA = INST_A(inst);
    v1 = vA;
    v2 = INST_B(inst);
    int retval = common_shift_int(shl_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode SHR_INT_2ADDR by calling common_shift_int

//!
int op_shr_int_2addr() {
    u2 vA, v1, v2;
    vA = INST_A(inst);
    v1 = vA;
    v2 = INST_B(inst);
    int retval = common_shift_int(sar_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode USHR_INT_2ADDR by calling common_shift_int

//!
int op_ushr_int_2addr() {
    u2 vA, v1, v2;
    vA = INST_A(inst);
    v1 = vA;
    v2 = INST_B(inst);
    int retval = common_shift_int(shr_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
#define P_GPR_1 PhysicalReg_EBX
//!common code to handle integer DIV & REM, it used GPR

//!The special case: when op0 == minint && op1 == -1, return 0 for isRem, return 0x80000000 for isDiv
//!There are two merge points in the control flow for this bytecode
//!make sure the reg. alloc. state is the same at merge points by calling transferToState
int common_div_rem_int(bool isRem, u2 vA, u2 v1, u2 v2) {
    get_virtual_reg(v1, OpndSize_32, PhysicalReg_EAX, true);
    get_virtual_reg(v2, OpndSize_32, 2, false);
    compare_imm_reg(OpndSize_32, 0, 2, false);
    handlePotentialException(
                                       Condition_E, Condition_NE,
                                       1, "common_errDivideByZero");
    /////////////////// handle special cases
    //conditional move 0 to $edx for rem for the two special cases
    //conditional move 0x80000000 to $eax for div
    //handle -1 special case divide error
    compare_imm_reg(OpndSize_32, -1, 2, false);
    conditional_jump(Condition_NE, ".common_div_rem_int_normal", true);
    //handle min int special case divide error
    rememberState(1);
    compare_imm_reg(OpndSize_32, 0x80000000, PhysicalReg_EAX, true);
    transferToState(1);
    conditional_jump(Condition_E, ".common_div_rem_int_special", true);

    insertLabel(".common_div_rem_int_normal", true); //merge point
    convert_integer(OpndSize_32, OpndSize_64); //cdq
    //idiv: dividend in edx:eax; quotient in eax; remainder in edx
    alu_unary_reg(OpndSize_32, idiv_opc, 2, false);
    if(isRem)
        set_virtual_reg(vA, OpndSize_32, PhysicalReg_EDX, true);
    else //divide: quotient in %eax
        set_virtual_reg(vA, OpndSize_32, PhysicalReg_EAX, true);
    rememberState(2);
    unconditional_jump(".common_div_rem_int_okay", true);

    insertLabel(".common_div_rem_int_special", true);
    goToState(1);
    if(isRem)
        set_VR_to_imm(vA, OpndSize_32, 0);
    else
        set_VR_to_imm(vA, OpndSize_32, 0x80000000);
    transferToState(2);
    insertLabel(".common_div_rem_int_okay", true); //merge point 2
    return 0;
}
#undef P_GPR_1
//! lower bytecode DIV_INT by calling common_div_rem_int

//!
int op_div_int() {
    u2 vA, v1, v2;
    vA = INST_AA(inst);
    v1 = *((u1*)rPC + 2);
    v2 = *((u1*)rPC + 3);
    int retval = common_div_rem_int(false, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode REM_INT by calling common_div_rem_int

//!
int op_rem_int() {
    u2 vA, v1, v2;
    vA = INST_AA(inst);
    v1 = *((u1*)rPC + 2);
    v2 = *((u1*)rPC + 3);
    int retval = common_div_rem_int(true, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode DIV_INT_2ADDR by calling common_div_rem_int

//!
int op_div_int_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_div_rem_int(false, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode REM_INT_2ADDR by calling common_div_rem_int

//!
int op_rem_int_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_div_rem_int(true, vA, v1, v2);
    rPC += 1;
    return retval;
}

#define P_GPR_1 PhysicalReg_EBX
//! common code to handle integer ALU ops with literal

//! It uses GPR
int common_alu_int_lit(ALU_Opcode opc, u2 vA, u2 vB, s2 imm) { //except div and rem
    get_virtual_reg(vB, OpndSize_32, 1, false);
    alu_binary_imm_reg(OpndSize_32, opc, imm, 1, false);
    set_virtual_reg(vA, OpndSize_32, 1, false);
    return 0;
}
//! calls common_alu_int_lit
int common_shift_int_lit(ALU_Opcode opc, u2 vA, u2 vB, s2 imm) {
    return common_alu_int_lit(opc, vA, vB, imm);
}
#undef p_GPR_1
//! lower bytecode ADD_INT_LIT16 by calling common_alu_int_lit

//!
int op_add_int_lit16() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    s4 tmp = (s2)FETCH(1);
    int retval = common_alu_int_lit(add_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}

int alu_rsub_int(ALU_Opcode opc, u2 vA, s2 imm, u2 vB) {
    move_imm_to_reg(OpndSize_32, imm, 2, false);
    get_virtual_reg(vB, OpndSize_32, 1, false);
    alu_binary_reg_reg(OpndSize_32, opc, 1, false, 2, false);
    set_virtual_reg(vA, OpndSize_32, 2, false);
    return 0;
}


//! lower bytecode RSUB_INT by calling common_alu_int_lit

//!
int op_rsub_int() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    s4 tmp = (s2)FETCH(1);
    int retval = alu_rsub_int(sub_opc, vA, tmp, vB);
    rPC += 2;
    return retval;
}
//! lower bytecode MUL_INT_LIT16 by calling common_alu_int_lit

//!
int op_mul_int_lit16() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    s4 tmp = (s2)FETCH(1);
    int retval = common_alu_int_lit(imul_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode AND_INT_LIT16 by calling common_alu_int_lit

//!
int op_and_int_lit16() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    s4 tmp = (s2)FETCH(1);
    int retval = common_alu_int_lit(and_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode OR_INT_LIT16 by calling common_alu_int_lit

//!
int op_or_int_lit16() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    s4 tmp = (s2)FETCH(1);
    int retval = common_alu_int_lit(or_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode XOR_INT_LIT16 by calling common_alu_int_lit

//!
int op_xor_int_lit16() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    s4 tmp = (s2)FETCH(1);
    int retval = common_alu_int_lit(xor_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode SHL_INT_LIT16 by calling common_shift_int_lit

//!
int op_shl_int_lit16() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    s4 tmp = (s2)FETCH(1);
    int retval = common_shift_int_lit(shl_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode SHR_INT_LIT16 by calling common_shift_int_lit

//!
int op_shr_int_lit16() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    s4 tmp = (s2)FETCH(1);
    int retval = common_shift_int_lit(sar_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode USHR_INT_LIT16 by calling common_shift_int_lit

//!
int op_ushr_int_lit16() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    s4 tmp = (s2)FETCH(1);
    int retval = common_shift_int_lit(shr_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode ADD_INT_LIT8 by calling common_alu_int_lit

//!
int op_add_int_lit8() {
    u2 vA = INST_AA(inst);
    u2 vB = (u2)FETCH(1) & 0xff;
    s2 tmp = (s2)FETCH(1) >> 8;
    int retval = common_alu_int_lit(add_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode RSUB_INT_LIT8 by calling common_alu_int_lit

//!
int op_rsub_int_lit8() {
    u2 vA = INST_AA(inst);
    u2 vB = (u2)FETCH(1) & 0xff;
    s2 tmp = (s2)FETCH(1) >> 8;
    int retval = alu_rsub_int(sub_opc, vA, tmp, vB);
    rPC += 2;
    return retval;
}
//! lower bytecode MUL_INT_LIT8 by calling common_alu_int_lit

//!
int op_mul_int_lit8() {
    u2 vA = INST_AA(inst);
    u2 vB = (u2)FETCH(1) & 0xff;
    s2 tmp = (s2)FETCH(1) >> 8;
    int retval = common_alu_int_lit(imul_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode AND_INT_LIT8 by calling common_alu_int_lit

//!
int op_and_int_lit8() {
    u2 vA = INST_AA(inst);
    u2 vB = (u2)FETCH(1) & 0xff;
    s2 tmp = (s2)FETCH(1) >> 8;
    int retval = common_alu_int_lit(and_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode OR_INT_LIT8 by calling common_alu_int_lit

//!
int op_or_int_lit8() {
    u2 vA = INST_AA(inst);
    u2 vB = (u2)FETCH(1) & 0xff;
    s2 tmp = (s2)FETCH(1) >> 8;
    int retval = common_alu_int_lit(or_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode XOR_INT_LIT8 by calling common_alu_int_lit

//!
int op_xor_int_lit8() {
    u2 vA = INST_AA(inst);
    u2 vB = (u2)FETCH(1) & 0xff;
    s2 tmp = (s2)FETCH(1) >> 8;
    int retval = common_alu_int_lit(xor_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode SHL_INT_LIT8 by calling common_shift_int_lit

//!
int op_shl_int_lit8() {
    u2 vA = INST_AA(inst);
    u2 vB = (u2)FETCH(1) & 0xff;
    s2 tmp = (s2)FETCH(1) >> 8;
    int retval = common_shift_int_lit(shl_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode SHR_INT_LIT8 by calling common_shift_int_lit

//!
int op_shr_int_lit8() {
    u2 vA = INST_AA(inst);
    u2 vB = (u2)FETCH(1) & 0xff;
    s2 tmp = (s2)FETCH(1) >> 8;
    int retval = common_shift_int_lit(sar_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode USHR_INT_LIT8 by calling common_shift_int_lit

//!
int op_ushr_int_lit8() {
    u2 vA = INST_AA(inst);
    u2 vB = (u2)FETCH(1) & 0xff;
    s2 tmp = (s2)FETCH(1) >> 8;
    int retval = common_shift_int_lit(shr_opc, vA, vB, tmp);
    rPC += 2;
    return retval;
}

int isPowerOfTwo(int imm) {
    int i;
    for(i = 1; i < 17; i++) {
        if(imm == (1 << i)) return i;
    }
    return -1;
}

#define P_GPR_1 PhysicalReg_EBX
int div_lit_strength_reduction(u2 vA, u2 vB, s2 imm) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        //strength reduction for div by 2,4,8,...
        int power = isPowerOfTwo(imm);
        if(power < 1) return 0;
        //tmp2 is not updated, so it can share with vB
        get_virtual_reg(vB, OpndSize_32, 2, false);
        //if imm is 2, power will be 1
        if(power == 1) {
            /* mov tmp1, tmp2
               shrl $31, tmp1
               addl tmp2, tmp1
               sarl $1, tmp1 */
            move_reg_to_reg(OpndSize_32, 2, false, 1, false);
            alu_binary_imm_reg(OpndSize_32, shr_opc, 31, 1, false);
            alu_binary_reg_reg(OpndSize_32, add_opc, 2, false, 1, false);
            alu_binary_imm_reg(OpndSize_32, sar_opc, 1, 1, false);
            set_virtual_reg(vA, OpndSize_32, 1, false);
            return 1;
        }
        //power > 1
        /* mov tmp1, tmp2
           sarl $power-1, tmp1
           shrl 32-$power, tmp1
           addl tmp2, tmp1
           sarl $power, tmp1 */
        move_reg_to_reg(OpndSize_32, 2, false, 1, false);
        alu_binary_imm_reg(OpndSize_32, sar_opc, power-1, 1, false);
        alu_binary_imm_reg(OpndSize_32, shr_opc, 32-power, 1, false);
        alu_binary_reg_reg(OpndSize_32, add_opc, 2, false, 1, false);
        alu_binary_imm_reg(OpndSize_32, sar_opc, power, 1, false);
        set_virtual_reg(vA, OpndSize_32, 1, false);
        return 1;
    }
    return 0;
}

////////// throws exception!!!
//! common code to handle integer DIV & REM with literal

//! It uses GPR
int common_div_rem_int_lit(bool isRem, u2 vA, u2 vB, s2 imm) {
    if(!isRem) {
        int retCode = div_lit_strength_reduction(vA, vB, imm);
        if(retCode > 0) return 0;
    }
    if(imm == 0) {
        export_pc(); //use %edx
#ifdef DEBUG_EXCEPTION
        LOGI("EXTRA code to handle exception");
#endif
        constVREndOfBB();
        beforeCall("exception"); //dump GG, GL VRs
        unconditional_jump_global_API(
                          "common_errDivideByZero", false);

        return 0;
    }
    get_virtual_reg(vB, OpndSize_32, PhysicalReg_EAX, true);
    //check against -1 for DIV_INT??
    if(imm == -1) {
        compare_imm_reg(OpndSize_32, 0x80000000, PhysicalReg_EAX, true);
        conditional_jump(Condition_E, ".div_rem_int_lit_special", true);
        rememberState(1);
    }
    move_imm_to_reg(OpndSize_32, imm, 2, false);
    convert_integer(OpndSize_32, OpndSize_64); //cdq
    //idiv: dividend in edx:eax; quotient in eax; remainder in edx
    alu_unary_reg(OpndSize_32, idiv_opc, 2, false);
    if(isRem)
        set_virtual_reg(vA, OpndSize_32, PhysicalReg_EDX, true);
    else
        set_virtual_reg(vA, OpndSize_32, PhysicalReg_EAX, true);

    if(imm == -1) {
        unconditional_jump(".div_rem_int_lit_okay", true);
        rememberState(2);
        insertLabel(".div_rem_int_lit_special", true);
        goToState(1);
        if(isRem)
            set_VR_to_imm(vA, OpndSize_32, 0);
        else
            set_VR_to_imm(vA, OpndSize_32, 0x80000000);
        transferToState(2);
    }

    insertLabel(".div_rem_int_lit_okay", true); //merge point 2
    return 0;
}
#undef P_GPR_1
//! lower bytecode DIV_INT_LIT16 by calling common_div_rem_int_lit

//!
int op_div_int_lit16() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    s4 tmp = (s2)FETCH(1);
    int retval = common_div_rem_int_lit(false, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode REM_INT_LIT16 by calling common_div_rem_int_lit

//!
int op_rem_int_lit16() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    s4 tmp = (s2)FETCH(1);
    int retval = common_div_rem_int_lit(true, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode DIV_INT_LIT8 by calling common_div_rem_int_lit

//!
int op_div_int_lit8() {
    u2 vA = INST_AA(inst);
    u2 vB = (u2)FETCH(1) & 0xff;
    s2 tmp = (s2)FETCH(1) >> 8;
    int retval = common_div_rem_int_lit(false, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! lower bytecode REM_INT_LIT8 by calling common_div_rem_int_lit

//!
int op_rem_int_lit8() {
    u2 vA = INST_AA(inst);
    u2 vB = (u2)FETCH(1) & 0xff;
    s2 tmp = (s2)FETCH(1) >> 8;
    int retval = common_div_rem_int_lit(true, vA, vB, tmp);
    rPC += 2;
    return retval;
}
//! common code to hanle long ALU ops

//! It uses XMM
int common_alu_long(ALU_Opcode opc, u2 vA, u2 v1, u2 v2) { //except div and rem
    get_virtual_reg(v1, OpndSize_64, 1, false);
    get_virtual_reg(v2, OpndSize_64, 2, false);
    alu_binary_reg_reg(OpndSize_64, opc, 2, false, 1, false);
    set_virtual_reg(vA, OpndSize_64, 1, false);
    return 0;
}
//! lower bytecode ADD_LONG by calling common_alu_long

//!
int op_add_long() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_long(add_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode SUB_LONG by calling common_alu_long

//!
int op_sub_long() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_long(sub_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode AND_LONG by calling common_alu_long

//!
int op_and_long() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_long(and_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode OR_LONG by calling common_alu_long

//!
int op_or_long() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_long(or_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode XOR_LONG by calling common_alu_long

//!
int op_xor_long() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_long(xor_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode ADD_LONG_2ADDR by calling common_alu_long

//!
int op_add_long_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_long(add_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode SUB_LONG_2ADDR by calling common_alu_long

//!
int op_sub_long_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_long(sub_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode AND_LONG_2ADDR by calling common_alu_long

//!
int op_and_long_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_long(and_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode OR_LONG_2ADDR by calling common_alu_long

//!
int op_or_long_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_long(or_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode XOR_LONG_2ADDR by calling common_alu_long

//!
int op_xor_long_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_long(xor_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}

//signed vs unsigned imul and mul?
#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
#define P_GPR_3 PhysicalReg_ESI
//! common code to handle multiplication of long

//! It uses GPR
int common_mul_long(u2 vA, u2 v1, u2 v2) {
    get_virtual_reg(v2, OpndSize_32, 1, false);
    move_reg_to_reg(OpndSize_32, 1, false, PhysicalReg_EAX, true);
    //imul: 2L * 1H update temporary 1
    alu_binary_VR_reg(OpndSize_32, imul_opc, (v1+1), 1, false);
    get_virtual_reg(v1, OpndSize_32, 3, false);
    move_reg_to_reg(OpndSize_32, 3, false, 2, false);
    //imul: 1L * 2H
    alu_binary_VR_reg(OpndSize_32, imul_opc, (v2+1), 2, false);
    alu_binary_reg_reg(OpndSize_32, add_opc, 2, false, 1, false);
    alu_unary_reg(OpndSize_32, mul_opc, 3, false);
    alu_binary_reg_reg(OpndSize_32, add_opc, PhysicalReg_EDX, true, 1, false);
    set_virtual_reg(vA+1, OpndSize_32, 1, false);
    set_virtual_reg(vA, OpndSize_32, PhysicalReg_EAX, true);
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_GPR_3
//! lower bytecode MUL_LONG by calling common_mul_long

//!
int op_mul_long() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_mul_long(vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode MUL_LONG_2ADDR by calling common_mul_long

//!
int op_mul_long_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_mul_long(vA, v1, v2);
    rPC += 1;
    return retval;
}

#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
//! common code to handle DIV & REM of long

//! It uses GPR & XMM; and calls call_moddi3 & call_divdi3
int common_div_rem_long(bool isRem, u2 vA, u2 v1, u2 v2) {
    get_virtual_reg(v2, OpndSize_32, 1, false);
    get_virtual_reg(v2+1, OpndSize_32, 2, false);
    //save to native stack before changing register P_GPR_1
    load_effective_addr(-16, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, 1, false, 8, PhysicalReg_ESP, true);
    alu_binary_reg_reg(OpndSize_32, or_opc, 2, false, 1, false);

    handlePotentialException(
                                       Condition_E, Condition_NE,
                                       1, "common_errDivideByZero");
    move_reg_to_mem(OpndSize_32, 2, false, 12, PhysicalReg_ESP, true);
    get_virtual_reg(v1, OpndSize_64, 1, false);
    move_reg_to_mem(OpndSize_64, 1, false, 0, PhysicalReg_ESP, true);
    scratchRegs[0] = PhysicalReg_SCRATCH_1;
    nextVersionOfHardReg(PhysicalReg_EDX, 2); //next version has 2 refs
    if(isRem)
        call_moddi3();
    else
        call_divdi3();
    load_effective_addr(16, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    set_virtual_reg(vA+1, OpndSize_32,PhysicalReg_EDX, true);
    set_virtual_reg(vA, OpndSize_32, PhysicalReg_EAX, true);
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2
//! lower bytecode DIV_LONG by calling common_div_rem_long

//!
int op_div_long() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_div_rem_long(false, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode REM_LONG by calling common_div_rem_long

//!
int op_rem_long() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_div_rem_long(true, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode DIV_LONG_2ADDR by calling common_div_rem_long

//!
int op_div_long_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_div_rem_long(false, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode REM_LONG_2ADDR by calling common_div_rem_long

//!
int op_rem_long_2addr() { //call __moddi3 instead of __divdi3
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_div_rem_long(true, vA, v1, v2);
    rPC += 1;
    return retval;
}

//! common code to handle SHL long

//! It uses XMM
int common_shl_long(u2 vA, u2 v1, u2 v2) {
    get_VR_ss(v2, 2, false);

    load_global_data_API("shiftMask", OpndSize_64, 3, false);

    get_virtual_reg(v1, OpndSize_64, 1, false);
    alu_binary_reg_reg(OpndSize_64, and_opc, 3, false, 2, false);
    alu_binary_reg_reg(OpndSize_64, sll_opc, 2, false, 1, false);
    set_virtual_reg(vA, OpndSize_64, 1, false);
    return 0;
}

//! common code to handle SHR long

//! It uses XMM
int common_shr_long(u2 vA, u2 v1, u2 v2) {
    get_VR_ss(v2, 2, false);

    load_global_data_API("shiftMask", OpndSize_64, 3, false);

    get_virtual_reg(v1, OpndSize_64, 1, false);
    alu_binary_reg_reg(OpndSize_64, and_opc, 3, false, 2, false);
    alu_binary_reg_reg(OpndSize_64, srl_opc, 2, false, 1, false);
    compare_imm_VR(OpndSize_32, 0, (v1+1));
    conditional_jump(Condition_GE, ".common_shr_long_special", true);
    rememberState(1);

    load_global_data_API("value64", OpndSize_64, 4, false);

    alu_binary_reg_reg(OpndSize_64, sub_opc, 2, false, 4, false);

    load_global_data_API("64bits", OpndSize_64, 5, false);

    alu_binary_reg_reg(OpndSize_64, sll_opc, 4, false, 5, false);
    alu_binary_reg_reg(OpndSize_64, or_opc, 5, false, 1, false);
    rememberState(2);
    //check whether the target is next instruction TODO
    unconditional_jump(".common_shr_long_done", true);

    insertLabel(".common_shr_long_special", true);
    goToState(1);
    transferToState(2);
    insertLabel(".common_shr_long_done", true);
    set_virtual_reg(vA, OpndSize_64, 1, false);
    return 0;
}

//! common code to handle USHR long

//! It uses XMM
int common_ushr_long(u2 vA, u2 v1, u2 v2) {
    get_VR_sd(v1, 1, false);
    get_VR_ss(v2, 2, false);

    load_sd_global_data_API("shiftMask", 3, false);

    alu_binary_reg_reg(OpndSize_64, and_opc, 3, false, 2, false);
    alu_binary_reg_reg(OpndSize_64, srl_opc, 2, false, 1, false);
    set_VR_sd(vA, 1, false);
    return 0;
}
//! lower bytecode SHL_LONG by calling common_shl_long

//!
int op_shl_long() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_shl_long(vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode SHL_LONG_2ADDR by calling common_shl_long

//!
int op_shl_long_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_shl_long(vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode SHR_LONG by calling common_shr_long

//!
int op_shr_long() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_shr_long(vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode SHR_LONG_2ADDR by calling common_shr_long

//!
int op_shr_long_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_shr_long(vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode USHR_LONG by calling common_ushr_long

//!
int op_ushr_long() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_ushr_long(vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode USHR_LONG_2ADDR by calling common_ushr_long

//!
int op_ushr_long_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_ushr_long(vA, v1, v2);
    rPC += 1;
    return retval;
}
#define USE_MEM_OPERAND
///////////////////////////////////////////
//! common code to handle ALU of floats

//! It uses XMM
int common_alu_float(ALU_Opcode opc, u2 vA, u2 v1, u2 v2) {//add, sub, mul
    get_VR_ss(v1, 1, false);
#ifdef USE_MEM_OPERAND
    alu_sd_binary_VR_reg(opc, v2, 1, false, false/*isSD*/);
#else
    get_VR_ss(v2, 2, false);
    alu_ss_binary_reg_reg(opc, 2, false, 1, false);
#endif
    set_VR_ss(vA, 1, false);
    return 0;
}
//! lower bytecode ADD_FLOAT by calling common_alu_float

//!
int op_add_float() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_float(add_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode SUB_FLOAT by calling common_alu_float

//!
int op_sub_float() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_float(sub_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode MUL_FLOAT by calling common_alu_float

//!
int op_mul_float() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_float(mul_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode ADD_FLOAT_2ADDR by calling common_alu_float

//!
int op_add_float_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_float(add_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode SUB_FLOAT_2ADDR by calling common_alu_float

//!
int op_sub_float_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_float(sub_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode MUL_FLOAT_2ADDR by calling common_alu_float

//!
int op_mul_float_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_float(mul_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! common code to handle DIV of float

//! It uses FP stack
int common_div_float(u2 vA, u2 v1, u2 v2) {
    load_fp_stack_VR(OpndSize_32, v1); //flds
    fpu_VR(div_opc, OpndSize_32, v2);
    store_fp_stack_VR(true, OpndSize_32, vA); //fstps
    return 0;
}
//! lower bytecode DIV_FLOAT by calling common_div_float

//!
int op_div_float() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_float(div_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode DIV_FLOAT_2ADDR by calling common_div_float

//!
int op_div_float_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_float(div_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! common code to handle DIV of double

//! It uses XMM
int common_alu_double(ALU_Opcode opc, u2 vA, u2 v1, u2 v2) {//add, sub, mul
    get_VR_sd(v1, 1, false);
#ifdef USE_MEM_OPERAND
    alu_sd_binary_VR_reg(opc, v2, 1, false, true /*isSD*/);
#else
    get_VR_sd(v2, 2, false);
    alu_sd_binary_reg_reg(opc, 2, false, 1, false);
#endif
    set_VR_sd(vA, 1, false);
    return 0;
}
//! lower bytecode ADD_DOUBLE by calling common_alu_double

//!
int op_add_double() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_double(add_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode SUB_DOUBLE by calling common_alu_double

//!
int op_sub_double() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_double(sub_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode MUL_DOUBLE by calling common_alu_double

//!
int op_mul_double() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_double(mul_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode ADD_DOUBLE_2ADDR by calling common_alu_double

//!
int op_add_double_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_double(add_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode SUB_DOUBLE_2ADDR by calling common_alu_double

//!
int op_sub_double_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_double(sub_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode MUL_DOUBLE_2ADDR by calling common_alu_double

//!
int op_mul_double_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_double(mul_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
//! common code to handle DIV of double

//! It uses FP stack
int common_div_double(u2 vA, u2 v1, u2 v2) {
    load_fp_stack_VR(OpndSize_64, v1); //fldl
    fpu_VR(div_opc, OpndSize_64, v2); //fdivl
    store_fp_stack_VR(true, OpndSize_64, vA); //fstpl
    return 0;
}
//! lower bytecode DIV_DOUBLE by calling common_div_double

//!
int op_div_double() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_alu_double(div_opc, vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode DIV_DOUBLE_2ADDR by calling common_div_double

//!
int op_div_double_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_alu_double(div_opc, vA, v1, v2);
    rPC += 1;
    return retval;
}
#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
//! common code to handle REM of float

//! It uses GPR & calls call_fmodf
int common_rem_float(u2 vA, u2 v1, u2 v2) {
    get_virtual_reg(v1, OpndSize_32, 1, false);
    get_virtual_reg(v2, OpndSize_32, 2, false);
    load_effective_addr(-8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, 1, false, 0, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, 2, false, 4, PhysicalReg_ESP, true);
    scratchRegs[0] = PhysicalReg_SCRATCH_1;
    call_fmodf(); //(float x, float y) return float
    load_effective_addr(8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    store_fp_stack_VR(true, OpndSize_32, vA); //fstps
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2
//! lower bytecode REM_FLOAT by calling common_rem_float

//!
int op_rem_float() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_rem_float(vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode REM_FLOAT_2ADDR by calling common_rem_float

//!
int op_rem_float_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_rem_float(vA, v1, v2);
    rPC += 1;
    return retval;
}
//! common code to handle REM of double

//! It uses XMM & calls call_fmod
int common_rem_double(u2 vA, u2 v1, u2 v2) {
    get_virtual_reg(v1, OpndSize_64, 1, false);
    get_virtual_reg(v2, OpndSize_64, 2, false);
    load_effective_addr(-16, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_64, 1, false, 0, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_64, 2, false, 8, PhysicalReg_ESP, true);
    scratchRegs[0] = PhysicalReg_SCRATCH_1;
    call_fmod(); //(long double x, long double y) return double
    load_effective_addr(16, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    store_fp_stack_VR(true, OpndSize_64, vA); //fstpl
    return 0;
}
//! lower bytecode REM_DOUBLE by calling common_rem_double

//!
int op_rem_double() {
    u2 vA = INST_AA(inst);
    u2 v1 = *((u1*)rPC + 2);
    u2 v2 = *((u1*)rPC + 3);
    int retval = common_rem_double(vA, v1, v2);
    rPC += 2;
    return retval;
}
//! lower bytecode REM_DOUBLE_2ADDR by calling common_rem_double

//!
int op_rem_double_2addr() {
    u2 vA = INST_A(inst);
    u2 v1 = vA;
    u2 v2 = INST_B(inst);
    int retval = common_rem_double(vA, v1, v2);
    rPC += 1;
    return retval;
}
//! lower bytecode CMPL_FLOAT

//!
int op_cmpl_float() {
    u2 vA = INST_AA(inst);
    u4 v1 = FETCH(1) & 0xff;
    u4 v2 = FETCH(1) >> 8;
    get_VR_ss(v1, 1, false); //xmm
    move_imm_to_reg(OpndSize_32, 0, 1, false);
    move_imm_to_reg(OpndSize_32, 1, 2, false);
    move_imm_to_reg(OpndSize_32, 0xffffffff, 3, false);
    compare_VR_ss_reg(v2, 1, false);
    //default: 0xffffffff??
    move_imm_to_reg(OpndSize_32,
                                 0xffffffff, 4, false);
    //ORDER of cmov matters !!! (Z,P,A)
    //finalNaN: unordered 0xffffffff
    conditional_move_reg_to_reg(OpndSize_32, Condition_Z,
                                             1, false, 4, false);
    conditional_move_reg_to_reg(OpndSize_32, Condition_P,
                                             3, false, 4, false);
    conditional_move_reg_to_reg(OpndSize_32, Condition_A,
                                             2, false, 4, false);
    set_virtual_reg(vA, OpndSize_32, 4, false);
    rPC += 2;
    return 0;
}
//! lower bytecode CMPG_FLOAT

//!
int op_cmpg_float() {
    u2 vA = INST_AA(inst);
    u4 v1 = FETCH(1) & 0xff;
    u4 v2 = FETCH(1) >> 8;
    get_VR_ss(v1, 1, false);
    compare_VR_ss_reg(v2, 1, false);
    move_imm_to_reg(OpndSize_32, 0, 1, false);
    move_imm_to_reg(OpndSize_32, 1, 2, false);
    //default: 0xffffffff??
    move_imm_to_reg(OpndSize_32, 0xffffffff, 3, false);
    conditional_move_reg_to_reg(OpndSize_32, Condition_Z,
                                1, false, 3, false);
    //finalNaN: unordered
    conditional_move_reg_to_reg(OpndSize_32, Condition_P,
                                2, false, 3, false);
    conditional_move_reg_to_reg(OpndSize_32, Condition_A,
                                2, false, 3, false);
    set_virtual_reg(vA, OpndSize_32, 3, false);
    rPC += 2;
    return 0;
}
//! lower bytecode CMPL_DOUBLE

//!
int op_cmpl_double() {
    u2 vA = INST_AA(inst);
    u4 v1 = FETCH(1) & 0xff;
    u4 v2 = FETCH(1) >> 8;
    get_VR_sd(v1, 1, false);
    compare_VR_sd_reg(v2, 1, false);
    move_imm_to_reg(OpndSize_32, 0, 1, false);
    move_imm_to_reg(OpndSize_32, 1, 2, false);
    move_imm_to_reg(OpndSize_32, 0xffffffff, 3, false);

    //default: 0xffffffff??
    move_imm_to_reg(OpndSize_32, 0xffffffff, 4, false);
    conditional_move_reg_to_reg(OpndSize_32, Condition_Z,
                                             1, false, 4, false);
    conditional_move_reg_to_reg(OpndSize_32, Condition_P,
                                             3, false, 4, false);
    conditional_move_reg_to_reg(OpndSize_32, Condition_A,
                                             2, false, 4, false);
    set_virtual_reg(vA, OpndSize_32, 4, false);
    rPC += 2;
    return 0;
}
//! lower bytecode CMPG_DOUBLE

//!
int op_cmpg_double() {
    u2 vA = INST_AA(inst);
    u4 v1 = FETCH(1) & 0xff;
    u4 v2 = FETCH(1) >> 8;
    get_VR_sd(v1, 1, false);
    compare_VR_sd_reg(v2, 1, false);
    move_imm_to_reg(OpndSize_32, 0, 1, false);
    move_imm_to_reg(OpndSize_32, 1, 2, false);

    //default: 0xffffffff??
    move_imm_to_reg(OpndSize_32,
                                 0xffffffff, 3, false);
    conditional_move_reg_to_reg(OpndSize_32, Condition_Z,
                                             1, false, 3, false);
    //finalNaN: unordered
    conditional_move_reg_to_reg(OpndSize_32, Condition_P,
                                             2, false, 3, false);
    conditional_move_reg_to_reg(OpndSize_32, Condition_A,
                                             2, false, 3, false);
   set_virtual_reg(vA, OpndSize_32, 3, false);
    rPC += 2;
    return 0;
}
#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
#define P_GPR_3 PhysicalReg_ESI
#define P_SCRATCH_1 PhysicalReg_EDX
#define P_SCRATCH_2 PhysicalReg_EAX
#define OPTION_OLD //for simpler cfg
//! lower bytecode CMP_LONG

//!
int op_cmp_long() {
    u2 vA = INST_AA(inst);
    u4 v1 = FETCH(1) & 0xff;
    u4 v2 = FETCH(1) >> 8;
    get_virtual_reg(v1+1, OpndSize_32, 2, false);
#ifdef OPTION_OLD
    move_imm_to_reg(OpndSize_32, 0xffffffff, 3, false);
    move_imm_to_reg(OpndSize_32, 1, 4, false);
    move_imm_to_reg(OpndSize_32, 0, 5, false);
#endif
    compare_VR_reg(OpndSize_32,
                                v2+1, 2, false);
#ifndef OPTION_OLD
    conditional_jump(Condition_L, ".cmp_long_less", true);
    conditional_jump(Condition_G, ".cmp_long_greater", true);
#else
    conditional_jump(Condition_E, ".cmp_long_equal", true);
    rememberState(1);
    conditional_move_reg_to_reg(OpndSize_32, Condition_L, //below vs less
                                             3, false, 6, false);
    conditional_move_reg_to_reg(OpndSize_32, Condition_G, //above vs greater
                                             4, false, 6, false);
    set_virtual_reg(vA, OpndSize_32, 6, false);
    rememberState(2);
    unconditional_jump(".cmp_long_okay", true);
    insertLabel(".cmp_long_equal", true);
    goToState(1);
#endif

    get_virtual_reg(v1, OpndSize_32, 1, false);
    compare_VR_reg(OpndSize_32,
                                v2, 1, false);
#ifdef OPTION_OLD
    conditional_move_reg_to_reg(OpndSize_32, Condition_E,
                                             5, false, 6, false);
    conditional_move_reg_to_reg(OpndSize_32, Condition_B, //below vs less
                                             3, false, 6, false);
    conditional_move_reg_to_reg(OpndSize_32, Condition_A, //above vs greater
                                             4, false, 6, false);
    set_virtual_reg(vA, OpndSize_32, 6, false);
    transferToState(2);
#else
    conditional_jump(Condition_A, ".cmp_long_greater", true);
    conditional_jump(Condition_NE, ".cmp_long_less", true);
    set_VR_to_imm(vA, OpndSize_32, 0);
    unconditional_jump(".cmp_long_okay", true);

    insertLabel(".cmp_long_less", true);
    set_VR_to_imm(vA, OpndSize_32, 0xffffffff);
    unconditional_jump(".cmp_long_okay", true);

    insertLabel(".cmp_long_greater", true);
    set_VR_to_imm(vA, OpndSize_32, 1);
#endif
    insertLabel(".cmp_long_okay", true);
    rPC += 2;
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_GPR_3
