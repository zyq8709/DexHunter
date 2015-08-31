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


/*! \file LowerConst.cpp
    \brief This file lowers the following bytecodes: CONST_XXX

    Functions are called from the lowered native sequence:
    1> const_string_resolve
       INPUT: const pool index in %eax
       OUTPUT: resolved string in %eax
       The only register that is still live after this function is ebx
    2> class_resolve
       INPUT: const pool index in %eax
       OUTPUT: resolved class in %eax
       The only register that is still live after this function is ebx
*/
#include "libdex/DexOpcodes.h"
#include "libdex/DexFile.h"
#include "Lower.h"
#include "NcgAot.h"
#include "enc_wrapper.h"

#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX

//! LOWER bytecode CONST_STRING without usage of helper function

//! It calls const_string_resolve (%ebx is live across the call)
//! Since the register allocator does not handle control flow within the lowered native sequence,
//!   we define an interface between the lowering module and register allocator:
//!     rememberState, gotoState, transferToState
//!   to make sure at the control flow merge point the state of registers is the same
int const_string_common_nohelper(u4 tmp, u2 vA) {
    /* for trace-based JIT, the string is already resolved since this code has been executed */
    void *strPtr = (void*)
              (currentMethod->clazz->pDvmDex->pResStrings[tmp]);
    assert(strPtr != NULL);
    set_VR_to_imm(vA, OpndSize_32, (int) strPtr );
    return 0;
}
//! dispatcher to select either const_string_common_helper or const_string_common_nohelper

//!
int const_string_common(u4 tmp, u2 vA) {
    return const_string_common_nohelper(tmp, vA);
}
#undef P_GPR_1
#undef P_GPR_2

//! lower bytecode CONST_4

//!
int op_const_4() {
    u2 vA = INST_A(inst);
    s4 tmp = (s4) (INST_B(inst) << 28) >> 28;
    set_VR_to_imm(vA, OpndSize_32, tmp);
    rPC += 1;
    return 1;
}
//! lower bytecode CONST_16

//!
int op_const_16() {
    u2 BBBB = FETCH(1);
    u2 vA = INST_AA(inst);
    set_VR_to_imm(vA, OpndSize_32, (s2)BBBB);
    rPC += 2;
    return 1;
}
//! lower bytecode CONST

//!
int op_const() {
    u2 vA = INST_AA(inst);
    u4 tmp = FETCH(1);
    tmp |= (u4)FETCH(2) << 16;
    set_VR_to_imm(vA, OpndSize_32, (s4)tmp);
    rPC += 3;
    return 1;
}
//! lower bytecode CONST_HIGH16

//!
int op_const_high16() {
    u2 vA = INST_AA(inst);
    u2 tmp = FETCH(1);
    set_VR_to_imm(vA, OpndSize_32, (s4)tmp<<16); //??
    rPC += 2;
    return 1;
}
//! lower bytecode CONST_WIDE_16

//!
int op_const_wide_16() {
    u2 vA = INST_AA(inst);
    u2 tmp = FETCH(1);
    set_VR_to_imm(vA, OpndSize_32, (s2)tmp);
    set_VR_to_imm(vA+1, OpndSize_32, (s2)tmp>>31);
    rPC += 2;
    return 2;
}
//! lower bytecode CONST_WIDE_32

//!
int op_const_wide_32() {
    u2 vA = INST_AA(inst);
    u4 tmp = FETCH(1);
    tmp |= (u4)FETCH(2) << 16;
    set_VR_to_imm(vA, OpndSize_32, (s4)tmp);
    set_VR_to_imm(vA+1, OpndSize_32, (s4)tmp>>31);
    rPC += 3;
    return 2;
}
//! lower bytecode CONST_WIDE

//!
int op_const_wide() {
    u2 vA = INST_AA(inst);
    u4 tmp = FETCH(1);
    tmp |= (u8)FETCH(2) << 16;
    set_VR_to_imm(vA, OpndSize_32, (s4)tmp);
    tmp = (u8)FETCH(3);
    tmp |= (u8)FETCH(4) << 16;
    set_VR_to_imm(vA+1, OpndSize_32, (s4)tmp);
    rPC += 5;
    return 2;
}
//! lower bytecode CONST_WIDE_HIGH16

//!
int op_const_wide_high16() {
    u2 vA = INST_AA(inst);
    u2 tmp = FETCH(1);
    set_VR_to_imm(vA, OpndSize_32, 0);
    set_VR_to_imm(vA+1, OpndSize_32, (s4)tmp<<16);
    rPC += 2;
    return 2;
}
//! lower bytecode CONST_STRING

//!
int op_const_string() {
    u2 vB = FETCH(1);
    u2 vA = INST_AA(inst);
    u4 tmp = vB;
    int retval = const_string_common(tmp, vA);
    rPC += 2;
    return retval;
}
//! lower bytecode CONST_STRING_JUMBO

//!
int op_const_string_jumbo() {
    u2 vA = INST_AA(inst);
    u4 tmp = FETCH(1);
    tmp |= (u4)FETCH(2) << 16;
    int retval = const_string_common(tmp, vA);
    rPC += 3;
    return retval;
}

#define P_GPR_1 PhysicalReg_EBX
//! LOWER bytecode CONST_CLASS

//! It calls class_resolve (%ebx is live across the call)
//! Since the register allocator does not handle control flow within the lowered native sequence,
//!   we define an interface between the lowering module and register allocator:
//!     rememberState, gotoState, transferToState
//!   to make sure at the control flow merge point the state of registers is the same
int op_const_class() {
    u2 vA = INST_AA(inst);
    u4 tmp = (u4)FETCH(1);
    /* for trace-based JIT, the class is already resolved since this code has been executed */
    void *classPtr = (void*)
       (currentMethod->clazz->pDvmDex->pResClasses[tmp]);
    assert(classPtr != NULL);
    set_VR_to_imm(vA, OpndSize_32, (int) classPtr );
    rPC += 2;
    return 0;
}

#undef P_GPR_1

