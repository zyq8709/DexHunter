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


/*! \file LowerGetPut.cpp
    \brief This file lowers the following bytecodes: XGET|PUT_XXX
*/
#include "libdex/DexOpcodes.h"
#include "libdex/DexFile.h"
#include "Lower.h"
#include "NcgAot.h"
#include "enc_wrapper.h"

#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
#define P_GPR_3 PhysicalReg_ESI
#define P_GPR_4 PhysicalReg_EDX
//! LOWER bytecode AGET without usage of helper function

//! It has null check and length check
int aget_common_nohelper(int flag, u2 vA, u2 vref, u2 vindex) {
    ////////////////////////////
    // Request VR free delays before register allocation for the temporaries
    if(!(traceCurrentMIR->OptimizationFlags & MIR_IGNORE_NULL_CHECK))
        requestVRFreeDelay(vref,VRDELAY_NULLCHECK);
    if(!(traceCurrentMIR->OptimizationFlags & MIR_IGNORE_RANGE_CHECK)) {
        requestVRFreeDelay(vref,VRDELAY_BOUNDCHECK);
        requestVRFreeDelay(vindex,VRDELAY_BOUNDCHECK);
    }

    get_virtual_reg(vref, OpndSize_32, 1, false); //array
    get_virtual_reg(vindex, OpndSize_32, 2, false); //index

    if(!(traceCurrentMIR->OptimizationFlags & MIR_IGNORE_NULL_CHECK)) {
        //last argument is the exception number for this bytecode
        nullCheck(1, false, 1, vref); //maybe optimized away, if not, call
        cancelVRFreeDelayRequest(vref,VRDELAY_NULLCHECK);
    } else {
        updateRefCount2(1, LowOpndRegType_gp, false); //update reference count for tmp1
    }

    if(!(traceCurrentMIR->OptimizationFlags & MIR_IGNORE_RANGE_CHECK)) {
        boundCheck(vref, 1, false,
                             vindex, 2, false,
                             2);
        cancelVRFreeDelayRequest(vref,VRDELAY_BOUNDCHECK);
        cancelVRFreeDelayRequest(vindex,VRDELAY_BOUNDCHECK);
    } else {
        updateRefCount2(1, LowOpndRegType_gp, false); //update reference count for tmp1
        updateRefCount2(2, LowOpndRegType_gp, false); //update reference count for tmp2
    }

    if(flag == AGET) {
        move_mem_disp_scale_to_reg(OpndSize_32, 1, false, offArrayObject_contents, 2, false, 4, 4, false);
    }
    else if(flag == AGET_WIDE) {
        move_mem_disp_scale_to_reg(OpndSize_64, 1, false, offArrayObject_contents, 2, false, 8, 1, false);
    }
    else if(flag == AGET_CHAR) {
        movez_mem_disp_scale_to_reg(OpndSize_16, 1, false, offArrayObject_contents, 2, false, 2, 4, false);
    }
    else if(flag == AGET_SHORT) {
        moves_mem_disp_scale_to_reg(OpndSize_16, 1, false, offArrayObject_contents, 2, false, 2, 4, false);
    }
    else if(flag == AGET_BOOLEAN) {
        movez_mem_disp_scale_to_reg(OpndSize_8, 1, false, offArrayObject_contents, 2, false, 1, 4, false);
    }
    else if(flag == AGET_BYTE) {
        moves_mem_disp_scale_to_reg(OpndSize_8, 1, false, offArrayObject_contents, 2, false, 1, 4, false);
    }
    if(flag == AGET_WIDE) {
        set_virtual_reg(vA, OpndSize_64, 1, false);
    }
    else {
        set_virtual_reg(vA, OpndSize_32, 4, false);
    }
    //////////////////////////////////
    return 0;
}
//! wrapper to call either aget_common_helper or aget_common_nohelper

//!
int aget_common(int flag, u2 vA, u2 vref, u2 vindex) {
    return aget_common_nohelper(flag, vA, vref, vindex);
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_GPR_3
#undef P_GPR_4
//! lower bytecode AGET by calling aget_common

//!
int op_aget() {
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;
    int retval = aget_common(AGET, vA, vref, vindex);
    rPC += 2;
    return retval;
}
//! lower bytecode AGET_WIDE by calling aget_common

//!
int op_aget_wide() {
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;
    int retval = aget_common(AGET_WIDE, vA, vref, vindex);
    rPC += 2;
    return retval;
}
//! lower bytecode AGET_OBJECT by calling aget_common

//!
int op_aget_object() {
    return op_aget();
}
//! lower bytecode BOOLEAN by calling aget_common

//!
int op_aget_boolean() {
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;
    int retval = aget_common(AGET_BOOLEAN, vA, vref, vindex);
    rPC += 2;
    return retval;
}
//! lower bytecode AGET_BYTE by calling aget_common

//!
int op_aget_byte() {
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;
    int retval = aget_common(AGET_BYTE, vA, vref, vindex);
    rPC += 2;
    return retval;
}
//! lower bytecode AGET_CHAR by calling aget_common

//!
int op_aget_char() {
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;
    int retval = aget_common(AGET_CHAR, vA, vref, vindex);
    rPC += 2;
    return retval;
}
//! lower bytecode AGET_SHORT by calling aget_common

//!
int op_aget_short() {
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;
    int retval = aget_common(AGET_SHORT, vA, vref, vindex);
    rPC += 2;
    return retval;
}

#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
#define P_GPR_3 PhysicalReg_ESI
#define P_GPR_4 PhysicalReg_EDX
//! LOWER bytecode APUT without usage of helper function

//! It has null check and length check
int aput_common_nohelper(int flag, u2 vA, u2 vref, u2 vindex) {
    //////////////////////////////////////
    // Request VR free delays before register allocation for the temporaries.
    // No need to request delay for vA since it will be transferred to temporary
    // after the null check and bound check.
    if(!(traceCurrentMIR->OptimizationFlags & MIR_IGNORE_NULL_CHECK))
        requestVRFreeDelay(vref,VRDELAY_NULLCHECK);
    if(!(traceCurrentMIR->OptimizationFlags & MIR_IGNORE_RANGE_CHECK)) {
        requestVRFreeDelay(vref,VRDELAY_BOUNDCHECK);
        requestVRFreeDelay(vindex,VRDELAY_BOUNDCHECK);
    }

    get_virtual_reg(vref, OpndSize_32, 1, false); //array
    get_virtual_reg(vindex, OpndSize_32, 2, false); //index

    if(!(traceCurrentMIR->OptimizationFlags & MIR_IGNORE_NULL_CHECK)) {
        //last argument is the exception number for this bytecode
        nullCheck(1, false, 1, vref); //maybe optimized away, if not, call
        cancelVRFreeDelayRequest(vref,VRDELAY_NULLCHECK);
    } else {
        updateRefCount2(1, LowOpndRegType_gp, false); //update reference count for tmp1
    }

    if(!(traceCurrentMIR->OptimizationFlags & MIR_IGNORE_RANGE_CHECK)) {
        boundCheck(vref, 1, false,
                             vindex, 2, false,
                             2);
        cancelVRFreeDelayRequest(vref,VRDELAY_BOUNDCHECK);
        cancelVRFreeDelayRequest(vindex,VRDELAY_BOUNDCHECK);
    } else {
        updateRefCount2(1, LowOpndRegType_gp, false); //update reference count for tmp1
        updateRefCount2(2, LowOpndRegType_gp, false); //update reference count for tmp2
    }

    if(flag == APUT_WIDE) {
        get_virtual_reg(vA, OpndSize_64, 1, false);
    }
    else {
        get_virtual_reg(vA, OpndSize_32, 4, false);
    }
    if(flag == APUT)
        move_reg_to_mem_disp_scale(OpndSize_32, 4, false, 1, false, offArrayObject_contents, 2, false, 4);
    else if(flag == APUT_WIDE)
        move_reg_to_mem_disp_scale(OpndSize_64, 1, false, 1, false, offArrayObject_contents, 2, false, 8);
    else if(flag == APUT_CHAR || flag == APUT_SHORT)
        move_reg_to_mem_disp_scale(OpndSize_16, 4, false, 1, false, offArrayObject_contents, 2, false, 2);
    else if(flag == APUT_BOOLEAN || flag == APUT_BYTE)
        move_reg_to_mem_disp_scale(OpndSize_8, 4, false, 1, false, offArrayObject_contents, 2, false, 1);
    //////////////////////////////////
    return 0;
}
//! wrapper to call either aput_common_helper or aput_common_nohelper

//!
int aput_common(int flag, u2 vA, u2 vref, u2 vindex) {
    return aput_common_nohelper(flag, vA, vref, vindex);
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_GPR_3
#undef P_GPR_4
//! lower bytecode APUT by calling aput_common

//!
int op_aput() {
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;
    int retval = aput_common(APUT, vA, vref, vindex);
    rPC += 2;
    return retval;
}
//! lower bytecode APUT_WIDE by calling aput_common

//!
int op_aput_wide() {
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;
    int retval = aput_common(APUT_WIDE, vA, vref, vindex);
    rPC += 2;
    return retval;
}
//! lower bytecode APUT_BOOLEAN by calling aput_common

//!
int op_aput_boolean() {
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;
    int retval = aput_common(APUT_BOOLEAN, vA, vref, vindex);
    rPC += 2;
    return retval;
}
//! lower bytecode APUT_BYTE by calling aput_common

//!
int op_aput_byte() {
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;
    int retval = aput_common(APUT_BYTE, vA, vref, vindex);
    rPC += 2;
    return retval;
}
//! lower bytecode APUT_CHAR by calling aput_common

//!
int op_aput_char() {
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;
    int retval = aput_common(APUT_CHAR, vA, vref, vindex);
    rPC += 2;
    return retval;
}
//! lower bytecode APUT_SHORT by calling aput_common

//!
int op_aput_short() {
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;
    int retval = aput_common(APUT_SHORT, vA, vref, vindex);
    rPC += 2;
    return retval;
}

#define P_GPR_1 PhysicalReg_EBX //callee-saved valid after CanPutArray
#define P_GPR_2 PhysicalReg_ECX
#define P_GPR_3 PhysicalReg_ESI //callee-saved
#define P_SCRATCH_1 PhysicalReg_EDX
#define P_SCRATCH_2 PhysicalReg_EAX
#define P_SCRATCH_3 PhysicalReg_EDX

void markCard_notNull(int tgtAddrReg, int scratchReg, bool isPhysical);

//! lower bytecode APUT_OBJECT

//! Lower the bytecode using helper function ".aput_obj_helper" if helper switch is on
int op_aput_object() { //type checking
    u2 vA = INST_AA(inst);
    u2 vref = FETCH(1) & 0xff;
    u2 vindex = FETCH(1) >> 8;

    ///////////////////////////
    // Request VR free delays before register allocation for the temporaries
    // No need to request delay for vA since it will be transferred to temporary
    // after the null check and bound check.
    if(!(traceCurrentMIR->OptimizationFlags & MIR_IGNORE_NULL_CHECK))
        requestVRFreeDelay(vref,VRDELAY_NULLCHECK);
    if(!(traceCurrentMIR->OptimizationFlags & MIR_IGNORE_RANGE_CHECK)) {
        requestVRFreeDelay(vref,VRDELAY_BOUNDCHECK);
        requestVRFreeDelay(vindex,VRDELAY_BOUNDCHECK);
    }

    get_virtual_reg(vref, OpndSize_32, 1, false); //array
    export_pc(); //use %edx

    if(!(traceCurrentMIR->OptimizationFlags & MIR_IGNORE_NULL_CHECK)) {
        compare_imm_reg(OpndSize_32, 0, 1, false);
        conditional_jump_global_API(Condition_E, "common_errNullObject", false);
        cancelVRFreeDelayRequest(vref,VRDELAY_NULLCHECK);
    } else {
        updateRefCount2(1, LowOpndRegType_gp, false); //update reference count for tmp1
    }

    get_virtual_reg(vindex, OpndSize_32, 2, false); //index
    if(!(traceCurrentMIR->OptimizationFlags & MIR_IGNORE_RANGE_CHECK)) {
        compare_mem_reg(OpndSize_32, offArrayObject_length, 1, false, 2, false);
        conditional_jump_global_API(Condition_NC, "common_errArrayIndex", false);
        cancelVRFreeDelayRequest(vref,VRDELAY_BOUNDCHECK);
        cancelVRFreeDelayRequest(vindex,VRDELAY_BOUNDCHECK);
    } else {
        updateRefCount2(1, LowOpndRegType_gp, false); //update reference count for tmp1
        updateRefCount2(2, LowOpndRegType_gp, false); //update reference count for tmp2
    }

    get_virtual_reg(vA, OpndSize_32, 4, false);
    compare_imm_reg(OpndSize_32, 0, 4, false);
    conditional_jump(Condition_E, ".aput_object_skip_check", true);
    rememberState(1);
    move_mem_to_reg(OpndSize_32, offObject_clazz, 4, false, 5, false);
    load_effective_addr(-12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, 5, false, 0, PhysicalReg_ESP, true);
    move_mem_to_reg(OpndSize_32, offObject_clazz, 1, false, 6, false);
    move_reg_to_mem(OpndSize_32, 6, false, 4, PhysicalReg_ESP, true);

    scratchRegs[0] = PhysicalReg_SCRATCH_1;
    call_dvmCanPutArrayElement(); //scratch??
    load_effective_addr(12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true);
    conditional_jump_global_API(Condition_E, "common_errArrayStore", false);

    //NOTE: "2, false" is live through function call
    move_reg_to_mem_disp_scale(OpndSize_32, 4, false, 1, false, offArrayObject_contents, 2, false, 4);
    markCard_notNull(1, 11, false);
    rememberState(2);
    ////TODO NCG O1 + code cache
    unconditional_jump(".aput_object_after_check", true);

    insertLabel(".aput_object_skip_check", true);
    goToState(1);
    //NOTE: "2, false" is live through function call
    move_reg_to_mem_disp_scale(OpndSize_32, 4, false, 1, false, offArrayObject_contents, 2, false, 4);

    transferToState(2);
    insertLabel(".aput_object_after_check", true);
    ///////////////////////////////
    rPC += 2;
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_GPR_3
#undef P_SCRATCH_1
#undef P_SCRATCH_2
#undef P_SCRATCH_3

//////////////////////////////////////////
#define P_GPR_1 PhysicalReg_ECX
#define P_GPR_2 PhysicalReg_EBX //should be callee-saved to avoid overwritten by inst_field_resolve
#define P_GPR_3 PhysicalReg_ESI
#define P_SCRATCH_1 PhysicalReg_EDX

/*
   movl offThread_cardTable(self), scratchReg
   compare_imm_reg 0, valReg (testl valReg, valReg)
   je .markCard_skip
   shrl $GC_CARD_SHIFT, tgtAddrReg
   movb %, (scratchReg, tgtAddrReg)
   NOTE: scratchReg can be accessed with the corresponding byte
         tgtAddrReg will be updated
   for O1, update the corresponding reference count
*/
void markCard(int valReg, int tgtAddrReg, bool targetPhysical, int scratchReg, bool isPhysical) {
   get_self_pointer(PhysicalReg_SCRATCH_6, isScratchPhysical);
   move_mem_to_reg(OpndSize_32, offsetof(Thread, cardTable), PhysicalReg_SCRATCH_6, isScratchPhysical, scratchReg, isPhysical);
   compare_imm_reg(OpndSize_32, 0, valReg, isPhysical);
   conditional_jump(Condition_E, ".markCard_skip", true);
   alu_binary_imm_reg(OpndSize_32, shr_opc, GC_CARD_SHIFT, tgtAddrReg, targetPhysical);
   move_reg_to_mem_disp_scale(OpndSize_8, scratchReg, isPhysical, scratchReg, isPhysical, 0, tgtAddrReg, targetPhysical, 1);
   insertLabel(".markCard_skip", true);
}

void markCard_notNull(int tgtAddrReg, int scratchReg, bool isPhysical) {
   get_self_pointer(PhysicalReg_SCRATCH_2, isScratchPhysical);
   move_mem_to_reg(OpndSize_32, offsetof(Thread, cardTable), PhysicalReg_SCRATCH_2, isScratchPhysical, scratchReg, isPhysical);
   alu_binary_imm_reg(OpndSize_32, shr_opc, GC_CARD_SHIFT, tgtAddrReg, isPhysical);
   move_reg_to_mem_disp_scale(OpndSize_8, scratchReg, isPhysical, scratchReg, isPhysical, 0, tgtAddrReg, isPhysical, 1);
}

void markCard_filled(int tgtAddrReg, bool isTgtPhysical, int scratchReg, bool isScratchPhysical) {
   get_self_pointer(PhysicalReg_SCRATCH_2, false/*isPhysical*/);
   move_mem_to_reg(OpndSize_32, offsetof(Thread, cardTable), PhysicalReg_SCRATCH_2, isScratchPhysical, scratchReg, isScratchPhysical);
   alu_binary_imm_reg(OpndSize_32, shr_opc, GC_CARD_SHIFT, tgtAddrReg, isTgtPhysical);
   move_reg_to_mem_disp_scale(OpndSize_8, scratchReg, isScratchPhysical, scratchReg, isScratchPhysical, 0, tgtAddrReg, isTgtPhysical, 1);
}
//! LOWER bytecode IGET,IPUT without usage of helper function

//! It has null check and calls assembly function inst_field_resolve
int iget_iput_common_nohelper(int tmp, int flag, u2 vA, u2 vB, int isObj, bool isVolatile) {
#ifdef WITH_JIT_INLINING
    const Method *method = (traceCurrentMIR->OptimizationFlags & MIR_CALLEE) ?
        traceCurrentMIR->meta.calleeMethod : currentMethod;
    InstField *pInstField = (InstField *)
            method->clazz->pDvmDex->pResFields[tmp];
#else
    InstField *pInstField = (InstField *)
            currentMethod->clazz->pDvmDex->pResFields[tmp];
#endif
    int fieldOffset;

    assert(pInstField != NULL);
    fieldOffset = pInstField->byteOffset;
    move_imm_to_reg(OpndSize_32, fieldOffset, 8, false);
    // Request VR delay before transfer to temporary. Only vB needs delay.
    // vA will have non-zero reference count since transfer to temporary for
    // it happens after null check, thus no delay is needed.
    requestVRFreeDelay(vB,VRDELAY_NULLCHECK);
    get_virtual_reg(vB, OpndSize_32, 7, false);
    nullCheck(7, false, 2, vB); //maybe optimized away, if not, call
    cancelVRFreeDelayRequest(vB,VRDELAY_NULLCHECK);
    if(flag == IGET) {
        move_mem_scale_to_reg(OpndSize_32, 7, false, 8, false, 1, 9, false);
        set_virtual_reg(vA, OpndSize_32, 9, false);
#ifdef DEBUG_IGET_OBJ
        if(isObj > 0) {
            pushAllRegs();
            load_effective_addr(-16, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
            move_reg_to_mem(OpndSize_32, 9, false, 12, PhysicalReg_ESP, true); //field
            move_reg_to_mem(OpndSize_32, 7, false, 8, PhysicalReg_ESP, true); //object
            move_imm_to_mem(OpndSize_32, tmp, 4, PhysicalReg_ESP, true); //field
            move_imm_to_mem(OpndSize_32, 0, 0, PhysicalReg_ESP, true); //iget
            call_dvmDebugIgetIput();
            load_effective_addr(16, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
            popAllRegs();
        }
#endif
    } else if(flag == IGET_WIDE) {
        if(isVolatile) {
            /* call dvmQuasiAtomicRead64(addr) */
            load_effective_addr(fieldOffset, 7, false, 9, false);
            move_reg_to_mem(OpndSize_32, 9, false, -4, PhysicalReg_ESP, true); //1st argument
            load_effective_addr(-4, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
            nextVersionOfHardReg(PhysicalReg_EAX, 2);
            nextVersionOfHardReg(PhysicalReg_EDX, 2);
            scratchRegs[0] = PhysicalReg_SCRATCH_3;
            call_dvmQuasiAtomicRead64();
            load_effective_addr(4, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
            //memory content in %edx, %eax
            set_virtual_reg(vA, OpndSize_32, PhysicalReg_EAX, true);
            set_virtual_reg(vA+1, OpndSize_32, PhysicalReg_EDX, true);
        } else {
            move_mem_scale_to_reg(OpndSize_64, 7, false, 8, false, 1, 1, false); //access field
            set_virtual_reg(vA, OpndSize_64, 1, false);
        }
    } else if(flag == IPUT) {
        get_virtual_reg(vA, OpndSize_32, 9, false);
        move_reg_to_mem_scale(OpndSize_32, 9, false, 7, false, 8, false, 1); //access field
        if(isObj) {
            markCard(9, 7, false, 11, false);
        }
    } else if(flag == IPUT_WIDE) {
        get_virtual_reg(vA, OpndSize_64, 1, false);
        if(isVolatile) {
            /* call dvmQuasiAtomicSwap64(val, addr) */
            load_effective_addr(fieldOffset, 7, false, 9, false);
            move_reg_to_mem(OpndSize_32, 9, false, -4, PhysicalReg_ESP, true); //2nd argument
            move_reg_to_mem(OpndSize_64, 1, false, -12, PhysicalReg_ESP, true); //1st argument
            load_effective_addr(-12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
            scratchRegs[0] = PhysicalReg_SCRATCH_3;
            call_dvmQuasiAtomicSwap64();
            load_effective_addr(12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
        }
        else {
            move_reg_to_mem_scale(OpndSize_64, 1, false, 7, false, 8, false, 1);
        }
    }
    ///////////////////////////
    return 0;
}
//! wrapper to call either iget_iput_common_helper or iget_iput_common_nohelper

//!
int iget_iput_common(int tmp, int flag, u2 vA, u2 vB, int isObj, bool isVolatile) {
    return iget_iput_common_nohelper(tmp, flag, vA, vB, isObj, isVolatile);
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_GPR_3
#undef P_SCRATCH_1
//! lower bytecode IGET by calling iget_iput_common

//!
int op_iget() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    u2 tmp = FETCH(1);
    int retval = iget_iput_common(tmp, IGET, vA, vB, 0, false);
    rPC += 2;
    return retval;
}
//! lower bytecode IGET_WIDE by calling iget_iput_common

//!
int op_iget_wide(bool isVolatile) {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    u2 tmp = FETCH(1);
    int retval = iget_iput_common(tmp, IGET_WIDE, vA, vB, 0, isVolatile);
    rPC += 2;
    return retval;
}
//! lower bytecode IGET_OBJECT by calling iget_iput_common

//!
int op_iget_object() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    u2 tmp = FETCH(1);
    int retval = iget_iput_common(tmp, IGET, vA, vB, 1, false);
    rPC += 2;
    return retval;
}
//! lower bytecode IGET_BOOLEAN by calling iget_iput_common

//!
int op_iget_boolean() {
    return op_iget();
}
//! lower bytecode IGET_BYTE by calling iget_iput_common

//!
int op_iget_byte() {
    return op_iget();
}
//! lower bytecode IGET_CHAR by calling iget_iput_common

//!
int op_iget_char() {
    return op_iget();
}
//! lower bytecode IGET_SHORT by calling iget_iput_common

//!
int op_iget_short() {
    return op_iget();
}
//! lower bytecode IPUT by calling iget_iput_common

//!
int op_iput() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    u2 tmp = FETCH(1);
    int retval = iget_iput_common(tmp, IPUT, vA, vB, 0, false);
    rPC += 2;
    return retval;
}
//! lower bytecode IPUT_WIDE by calling iget_iput_common

//!
int op_iput_wide(bool isVolatile) {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    u2 tmp = FETCH(1);
    int retval = iget_iput_common(tmp, IPUT_WIDE, vA, vB, 0, isVolatile);
    rPC += 2;
    return retval;
}
//! lower bytecode IPUT_OBJECT by calling iget_iput_common

//!
int op_iput_object() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    u2 tmp = FETCH(1);
    int retval = iget_iput_common(tmp, IPUT, vA, vB, 1, false);
    rPC += 2;
    return retval;
}
//! lower bytecode IPUT_BOOLEAN by calling iget_iput_common

//!
int op_iput_boolean() {
    return op_iput();
}
//! lower bytecode IPUT_BYTE by calling iget_iput_common

//!
int op_iput_byte() {
    return op_iput();
}
//! lower bytecode IPUT_CHAR by calling iget_iput_common

//!
int op_iput_char() {
    return op_iput();
}
//! lower bytecode IPUT_SHORT by calling iget_iput_common

//!
int op_iput_short() {
    return op_iput();
}

#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
#define P_GPR_3 PhysicalReg_EDX //used by helper only

//! common section to lower IGET & IPUT

//! It will use helper function sget_helper if the switch is on
int sget_sput_common(int flag, u2 vA, u2 tmp, bool isObj, bool isVolatile) {
    //call assembly static_field_resolve
    //no exception
    //glue: get_res_fields
    //hard-coded: eax (one version?)
    //////////////////////////////////////////
#ifdef WITH_JIT_INLINING
    const Method *method = (traceCurrentMIR->OptimizationFlags & MIR_CALLEE) ? traceCurrentMIR->meta.calleeMethod : currentMethod;
    void *fieldPtr = (void*)
        (method->clazz->pDvmDex->pResFields[tmp]);
#else
    void *fieldPtr = (void*)
        (currentMethod->clazz->pDvmDex->pResFields[tmp]);
#endif

    /* Usually, fieldPtr should not be null. The interpreter should resolve
     * it before we come here, or not allow this opcode in a trace. However,
     * we can be in a loop trace and this opcode might have been picked up
     * by exhaustTrace. Sending a -1 here will terminate the loop formation
     * and fall back to normal trace, which will not have this opcode.
     */
    if (!fieldPtr) {
        return -1;
    }

    move_imm_to_reg(OpndSize_32, (int)fieldPtr, PhysicalReg_EAX, true);
    if(flag == SGET) {
        move_mem_to_reg(OpndSize_32, offStaticField_value, PhysicalReg_EAX, true, 7, false); //access field
        set_virtual_reg(vA, OpndSize_32, 7, false);
    } else if(flag == SGET_WIDE) {
        if(isVolatile) {
            /* call dvmQuasiAtomicRead64(addr) */
            load_effective_addr(offStaticField_value, PhysicalReg_EAX, true, 9, false);
            move_reg_to_mem(OpndSize_32, 9, false, -4, PhysicalReg_ESP, true); //1st argument
            load_effective_addr(-4, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
            nextVersionOfHardReg(PhysicalReg_EAX, 2);
            nextVersionOfHardReg(PhysicalReg_EDX, 2);
            scratchRegs[0] = PhysicalReg_SCRATCH_3;
            call_dvmQuasiAtomicRead64();
            load_effective_addr(4, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
            //memory content in %edx, %eax
            set_virtual_reg(vA, OpndSize_32, PhysicalReg_EAX, true);
            set_virtual_reg(vA+1, OpndSize_32, PhysicalReg_EDX, true);
        }
        else {
            move_mem_to_reg(OpndSize_64, offStaticField_value, PhysicalReg_EAX, true, 1, false); //access field
            set_virtual_reg(vA, OpndSize_64, 1, false);
        }
    } else if(flag == SPUT) {
        get_virtual_reg(vA, OpndSize_32, 7, false);
        move_reg_to_mem(OpndSize_32, 7, false, offStaticField_value, PhysicalReg_EAX, true); //access field
        if(isObj) {
            /* get clazz object, then use clazz object to mark card */
            move_mem_to_reg(OpndSize_32, offField_clazz, PhysicalReg_EAX, true, 12, false);
            markCard(7/*valReg*/, 12, false, 11, false);
        }
    } else if(flag == SPUT_WIDE) {
        get_virtual_reg(vA, OpndSize_64, 1, false);
        if(isVolatile) {
            /* call dvmQuasiAtomicSwap64(val, addr) */
            load_effective_addr(offStaticField_value, PhysicalReg_EAX, true, 9, false);
            move_reg_to_mem(OpndSize_32, 9, false, -4, PhysicalReg_ESP, true); //2nd argument
            move_reg_to_mem(OpndSize_64, 1, false, -12, PhysicalReg_ESP, true); //1st argument
            load_effective_addr(-12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
            scratchRegs[0] = PhysicalReg_SCRATCH_3;
            call_dvmQuasiAtomicSwap64();
            load_effective_addr(12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
        }
        else {
            move_reg_to_mem(OpndSize_64, 1, false, offStaticField_value, PhysicalReg_EAX, true); //access field
        }
    }
    //////////////////////////////////////////////
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_GPR_3
//! lower bytecode SGET by calling sget_sput_common

//!
int op_sget() {
    u2 vA = INST_AA(inst);
    u2 tmp = FETCH(1);
    int retval = sget_sput_common(SGET, vA, tmp, false, false);
    rPC += 2;
    return retval;
}
//! lower bytecode SGET_WIDE by calling sget_sput_common

//!
int op_sget_wide(bool isVolatile) {
    u2 vA = INST_AA(inst);
    u2 tmp = FETCH(1);
    int retval = sget_sput_common(SGET_WIDE, vA, tmp, false, isVolatile);
    rPC += 2;
    return retval;
}
//! lower bytecode SGET_OBJECT by calling sget_sput_common

//!
int op_sget_object() {
    return op_sget();
}
//! lower bytecode SGET_BOOLEAN by calling sget_sput_common

//!
int op_sget_boolean() {
    return op_sget();
}
//! lower bytecode SGET_BYTE by calling sget_sput_common

//!
int op_sget_byte() {
    return op_sget();
}
//! lower bytecode SGET_CHAR by calling sget_sput_common

//!
int op_sget_char() {
    return op_sget();
}
//! lower bytecode SGET_SHORT by calling sget_sput_common

//!
int op_sget_short() {
    return op_sget();
}
//! lower bytecode SPUT by calling sget_sput_common

//!
int op_sput(bool isObj) {
    u2 vA = INST_AA(inst);
    u2 tmp = FETCH(1);
    int retval = sget_sput_common(SPUT, vA, tmp, isObj, false);
    rPC += 2;
    return retval;
}
//! lower bytecode SPUT_WIDE by calling sget_sput_common

//!
int op_sput_wide(bool isVolatile) {
    u2 vA = INST_AA(inst);
    u2 tmp = FETCH(1);
    int retval = sget_sput_common(SPUT_WIDE, vA, tmp, false, isVolatile);
    rPC += 2;
    return retval;
}
//! lower bytecode SPUT_OBJECT by calling sget_sput_common

//!
int op_sput_object() {
    return op_sput(true);
}
//! lower bytecode SPUT_OBJECT by calling sget_sput_common

//!
int op_sput_boolean() {
    return op_sput(false);
}
//! lower bytecode SPUT_BOOLEAN by calling sget_sput_common

//!
int op_sput_byte() {
    return op_sput(false);
}
//! lower bytecode SPUT_BYTE by calling sget_sput_common

//!
int op_sput_char() {
    return op_sput(false);
}
//! lower bytecode SPUT_SHORT by calling sget_sput_common

//!
int op_sput_short() {
    return op_sput(false);
}
#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
//! lower bytecode IGET_QUICK

//!
int op_iget_quick() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst); //object
    u2 tmp = FETCH(1);

    requestVRFreeDelay(vB,VRDELAY_NULLCHECK); // Request VR delay before transfer to temporary
    get_virtual_reg(vB, OpndSize_32, 1, false);
    nullCheck(1, false, 1, vB); //maybe optimized away, if not, call
    cancelVRFreeDelayRequest(vB,VRDELAY_NULLCHECK);

    move_mem_to_reg(OpndSize_32, tmp, 1, false, 2, false);
    set_virtual_reg(vA, OpndSize_32, 2, false);
    rPC += 2;
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2
#define P_GPR_1 PhysicalReg_EBX
//! lower bytecode IGET_WIDE_QUICK

//!
int op_iget_wide_quick() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst); //object
    u2 tmp = FETCH(1);

    requestVRFreeDelay(vB,VRDELAY_NULLCHECK); // Request VR delay before transfer to temporary
    get_virtual_reg(vB, OpndSize_32, 1, false);
    nullCheck(1, false, 1, vB); //maybe optimized away, if not, call
    cancelVRFreeDelayRequest(vB,VRDELAY_NULLCHECK);

    move_mem_to_reg(OpndSize_64, tmp, 1, false, 1, false);
    set_virtual_reg(vA, OpndSize_64, 1, false);
    rPC += 2;
    return 0;
}
#undef P_GPR_1
//! lower bytecode IGET_OBJECT_QUICK

//!
int op_iget_object_quick() {
    return op_iget_quick();
}
#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
//! lower bytecode IPUT_QUICK

//!
int iput_quick_common(bool isObj) {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst); //object
    u2 tmp = FETCH(1);

    // Request VR delay before transfer to temporary. Only vB needs delay.
    // vA will have non-zero reference count since transfer to temporary for
    // it happens after null check, thus no delay is needed.
    requestVRFreeDelay(vB,VRDELAY_NULLCHECK);
    get_virtual_reg(vB, OpndSize_32, 1, false);
    nullCheck(1, false, 1, vB); //maybe optimized away, if not, call
    cancelVRFreeDelayRequest(vB,VRDELAY_NULLCHECK);

    get_virtual_reg(vA, OpndSize_32, 2, false);
    move_reg_to_mem(OpndSize_32, 2, false, tmp, 1, false);
    if(isObj) {
        markCard(2/*valReg*/, 1, false, 11, false);
    }
    rPC += 2;
    return 0;
}
int op_iput_quick() {
    return iput_quick_common(false);
}
#undef P_GPR_1
#undef P_GPR_2
#define P_GPR_1 PhysicalReg_EBX
//! lower bytecode IPUT_WIDE_QUICK

//!
int op_iput_wide_quick() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst); //object
    u2 tmp = FETCH(1); //byte offset

    // Request VR delay before transfer to temporary. Only vB needs delay.
    // vA will have non-zero reference count since transfer to temporary for
    // it happens after null check, thus no delay is needed.
    requestVRFreeDelay(vB,VRDELAY_NULLCHECK);
    get_virtual_reg(vB, OpndSize_32, 1, false);
    nullCheck(1, false, 1, vB); //maybe optimized away, if not, call
    cancelVRFreeDelayRequest(vB,VRDELAY_NULLCHECK);

    get_virtual_reg(vA, OpndSize_64, 1, false);
    move_reg_to_mem(OpndSize_64, 1, false, tmp, 1, false);
    rPC += 2;
    return 0;
}
#undef P_GPR_1
//! lower bytecode IPUT_OBJECT_QUICK

//!
int op_iput_object_quick() {
    return iput_quick_common(true);
}

