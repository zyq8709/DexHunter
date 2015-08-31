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

/*! \file LowerObject.cpp
    \brief This file lowers the following bytecodes: CHECK_CAST,
*/
#include "libdex/DexOpcodes.h"
#include "libdex/DexFile.h"
#include "Lower.h"
#include "NcgAot.h"
#include "enc_wrapper.h"

extern void markCard_filled(int tgtAddrReg, bool isTgtPhysical, int scratchReg, bool isScratchPhysical);

#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
#define P_GPR_3 PhysicalReg_ESI
//! LOWER bytecode CHECK_CAST and INSTANCE_OF
//!   CALL class_resolve (%ebx is live across the call)
//!        dvmInstanceofNonTrivial
//!   NO register is live through function check_cast_helper
int check_cast_nohelper(u2 vA, u4 tmp, bool instance, u2 vDest) {
    get_virtual_reg(vA, OpndSize_32, 1, false); //object
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;
    /* for trace-based JIT, it is likely that the class is already resolved */
    bool needToResolve = true;
    ClassObject *classPtr =
                (currentMethod->clazz->pDvmDex->pResClasses[tmp]);
    ALOGV("in check_cast, class is resolved to %p", classPtr);
    if(classPtr != NULL) {
        needToResolve = false;
        ALOGV("check_cast class %s", classPtr->descriptor);
    }
    if(needToResolve) {
        //get_res_classes is moved here for NCG O1 to improve performance of GLUE optimization
        scratchRegs[0] = PhysicalReg_SCRATCH_1; scratchRegs[1] = PhysicalReg_SCRATCH_2;
        get_res_classes(4, false);
    }
    compare_imm_reg(OpndSize_32, 0, 1, false);

    rememberState(1);
    //for private code cache, previously it jumped to .instance_of_okay_1
    //if object reference is null, jump to the handler for this special case
    if(instance) {
        conditional_jump(Condition_E, ".instance_of_null", true);
    }
    else {
        conditional_jump(Condition_E, ".check_cast_null", true);
    }
    //check whether the class is already resolved
    //if yes, jump to check_cast_resolved
    //if not, call class_resolve
    if(needToResolve) {
        move_mem_to_reg(OpndSize_32, tmp*4, 4, false, PhysicalReg_EAX, true);
        compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true);
        if(instance)
            conditional_jump(Condition_NE, ".instance_of_resolved", true);
        else
            conditional_jump(Condition_NE, ".check_cast_resolved", true);
        //try to resolve the class
        rememberState(2);
        move_imm_to_reg(OpndSize_32, tmp, PhysicalReg_EAX, true);
        export_pc(); //trying to resolve the class
        call_helper_API(".class_resolve");
        transferToState(2);
    } //needToResolve
    else {
        /* the class is already resolved and is constant */
        move_imm_to_reg(OpndSize_32, (int)classPtr, PhysicalReg_EAX, true);
    }
    //class is resolved, and it is in %eax
    if(!instance) {
        insertLabel(".check_cast_resolved", true);
    }
    else insertLabel(".instance_of_resolved", true);

    move_mem_to_reg(OpndSize_32, offObject_clazz, 1, false, 6, false); //object->clazz

    //%eax: resolved class
    //compare resolved class and object->clazz
    //if the same, jump to the handler for this special case
    compare_reg_reg(PhysicalReg_EAX, true, 6, false);
    rememberState(3);
    if(instance) {
        conditional_jump(Condition_E, ".instance_of_equal", true);
    } else {
        conditional_jump(Condition_E, ".check_cast_equal", true);
    }

    //prepare to call dvmInstanceofNonTrivial
    //INPUT: the resolved class & object reference
    load_effective_addr(-8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, 6, false, 0, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, PhysicalReg_EAX, true, 4, PhysicalReg_ESP, true); //resolved class
    scratchRegs[0] = PhysicalReg_SCRATCH_3;
    nextVersionOfHardReg(PhysicalReg_EAX, 2); //next version has 2 refs
    call_dvmInstanceofNonTrivial();
    load_effective_addr(8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    //
    if(instance) {
        //move return value to P_GPR_2
        move_reg_to_reg(OpndSize_32, PhysicalReg_EAX, true, 3, false);
        rememberState(4);
        unconditional_jump(".instance_of_okay", true);
    } else {
        //if return value of dvmInstanceofNonTrivial is zero, throw exception
        compare_imm_reg(OpndSize_32, 0,  PhysicalReg_EAX, true);
        rememberState(4);
        conditional_jump(Condition_NE, ".check_cast_okay", true);
        //two inputs for common_throw_message: object reference in eax, exception pointer in ecx
        nextVersionOfHardReg(PhysicalReg_EAX, 1); //next version has 1 ref
        move_reg_to_reg(OpndSize_32, 1, false, PhysicalReg_EAX, true);

        load_imm_global_data_API("strClassCastExceptionPtr", OpndSize_32, PhysicalReg_ECX, true);

        nextVersionOfHardReg(PhysicalReg_EDX, 2); //next version has 2 ref count
        export_pc();

        unconditional_jump_global_API("common_throw_message", false);
    }
    //handler for speical case where object reference is null
    if(instance)
        insertLabel(".instance_of_null", true);
    else insertLabel(".check_cast_null", true);
    goToState(1);
    if(instance) {
        move_imm_to_reg(OpndSize_32, 0, 3, false);
    }
    transferToState(4);
    if(instance)
        unconditional_jump(".instance_of_okay", true);
    else
        unconditional_jump(".check_cast_okay", true);

    //handler for special case where class of object is the same as the resolved class
    if(instance)
        insertLabel(".instance_of_equal", true);
    else insertLabel(".check_cast_equal", true);
    goToState(3);
    if(instance) {
        move_imm_to_reg(OpndSize_32, 1, 3, false);
    }
    transferToState(4);
    if(instance)
        insertLabel(".instance_of_okay", true);
    else insertLabel(".check_cast_okay", true);
    //all cases merge here and the value is put to virtual register
    if(instance) {
        set_virtual_reg(vDest, OpndSize_32, 3, false);
    }
    return 0;
}
//! common code to lower CHECK_CAST & INSTANCE_OF

//!
int common_check_cast_instance_of(u2 vA, u4 tmp, bool instance, u2 vDest) {
    return check_cast_nohelper(vA, tmp, instance, vDest);
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_GPR_3

//! LOWER bytecode CHECK_CAST

//!
int op_check_cast() {
    u2 vA = INST_AA(inst);
    u4 tmp = (u4)FETCH(1);
    common_check_cast_instance_of(vA, tmp, false, 0);
    rPC += 2;
    return 0;
}
//!LOWER bytecode INSTANCE_OF

//!
int op_instance_of() {
    u2 vB = INST_B(inst);
    u2 vA = INST_A(inst);
    u4 tmp = (u4)FETCH(1);
    common_check_cast_instance_of(vB, tmp, true, vA);
    rPC += 2;
    return 0;
}

#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
//! LOWER bytecode MONITOR_ENTER without usage of helper function

//!   CALL dvmLockObject
int monitor_enter_nohelper(u2 vA) {
    scratchRegs[0] = PhysicalReg_SCRATCH_1;
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;

    requestVRFreeDelay(vA,VRDELAY_NULLCHECK); // Request VR delay before transfer to temporary
    //get_self_pointer is separated
    get_virtual_reg(vA, OpndSize_32, 1, false);
    //to optimize redundant null check, NCG O1 wraps up null check in a function: nullCheck
    get_self_pointer(3, false);
    nullCheck(1, false, 1, vA); //maybe optimized away
    cancelVRFreeDelayRequest(vA,VRDELAY_NULLCHECK);

    /////////////////////////////
    //prepare to call dvmLockObject, inputs: object reference and self
    // TODO: Should reset inJitCodeCache before calling dvmLockObject
    //       so that code cache can be reset if needed when locking object
    //       taking a long time. Not resetting inJitCodeCache may delay
    //       code cache reset when code cache is full, preventing traces from
    //       JIT compilation. This has performance implication.
    //       However, after resetting inJitCodeCache, the code should be
    //       wrapped in a helper instead of directly inlined in code cache.
    //       If the code after dvmLockObject call is in code cache and the code
    //       cache is reset during dvmLockObject call, execution after
    //       dvmLockObject will return to a cleared code cache region,
    //       resulting in seg fault.
    load_effective_addr(-8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, 1, false, 4, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, 3, false, 0, PhysicalReg_ESP, true);
    scratchRegs[0] = PhysicalReg_SCRATCH_2;
    call_dvmLockObject();
    load_effective_addr(8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    /////////////////////////////
    return 0;
}
//! lower bytecode MONITOR_ENTER

//! It will use helper function if switch is on
int op_monitor_enter() {
    u2 vA = INST_AA(inst);
    export_pc();
    monitor_enter_nohelper(vA);
    rPC += 1;
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2

#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
//! lower bytecode MONITOR_EXIT

//! It will use helper function if switch is on
int op_monitor_exit() {
    u2 vA = INST_AA(inst);
    ////////////////////
    //LOWER bytecode MONITOR_EXIT without helper function
    //   CALL dvmUnlockObject
    scratchRegs[0] = PhysicalReg_SCRATCH_1; scratchRegs[1] = PhysicalReg_SCRATCH_2;
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;
    requestVRFreeDelay(vA,VRDELAY_NULLCHECK); // Request VR delay before transfer to temporary
    get_virtual_reg(vA, OpndSize_32, 1, false);
    nullCheck(1, false, 1, vA); //maybe optimized away
    cancelVRFreeDelayRequest(vA,VRDELAY_NULLCHECK);

    /////////////////////////////
    //prepare to call dvmUnlockObject, inputs: object reference and self
    push_reg_to_stack(OpndSize_32, 1, false);
    push_mem_to_stack(OpndSize_32, offEBP_self, PhysicalReg_EBP, true);
    scratchRegs[0] = PhysicalReg_SCRATCH_2;
    call_dvmUnlockObject();
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true);
    load_effective_addr(8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);

    conditional_jump(Condition_NE, ".unlock_object_done", true);
    //jump to dvmJitToExceptionThrown
    scratchRegs[0] = PhysicalReg_SCRATCH_3;
    jumpToExceptionThrown(2/*exception number*/);
    insertLabel(".unlock_object_done", true);
    ///////////////////////////
    rPC += 1;
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2

#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
#define P_GPR_3 PhysicalReg_EDX /*vA*/
//! LOWER bytecode ARRAY_LENGTH

//! It will use helper function if switch is on
int op_array_length() {
    u2 vA = INST_A(inst);
    u2 vB = INST_B(inst);
    ////////////////////
    //no usage of helper function
    requestVRFreeDelay(vB,VRDELAY_NULLCHECK); // Request VR delay before transfer to temporary
    get_virtual_reg(vB, OpndSize_32, 1, false);
    nullCheck(1, false, 1, vB); //maybe optimized away
    cancelVRFreeDelayRequest(vB,VRDELAY_NULLCHECK);

    move_mem_to_reg(OpndSize_32, offArrayObject_length, 1, false, 2, false);
    set_virtual_reg(vA, OpndSize_32, 2, false);
    ///////////////////////
    rPC += 1;
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_GPR_3

#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
#define P_GPR_3 PhysicalReg_ESI
//! lower bytecode NEW_INSTANCE

//! It will use helper function if switch is on
int op_new_instance() {
    u4 tmp = (u4)FETCH(1);
    u2 vA = INST_AA(inst);
    export_pc();
    /* for trace-based JIT, class is already resolved */
    ClassObject *classPtr =
        (currentMethod->clazz->pDvmDex->pResClasses[tmp]);
    assert(classPtr != NULL);
    assert(classPtr->status & CLASS_INITIALIZED);
    /*
     * If it is going to throw, it should not make to the trace to begin
     * with.  However, Alloc might throw, so we need to genExportPC()
     */
    assert((classPtr->accessFlags & (ACC_INTERFACE|ACC_ABSTRACT)) == 0);
    //prepare to call dvmAllocObject, inputs: resolved class & flag ALLOC_DONT_TRACK
    load_effective_addr(-8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    /* 1st argument to dvmAllocObject at -8(%esp) */
    move_imm_to_mem(OpndSize_32, (int)classPtr, 0, PhysicalReg_ESP, true);
    move_imm_to_mem(OpndSize_32, ALLOC_DONT_TRACK, 4, PhysicalReg_ESP, true);
    scratchRegs[0] = PhysicalReg_SCRATCH_3;
    nextVersionOfHardReg(PhysicalReg_EAX, 3); //next version has 3 refs
    call_dvmAllocObject();
    load_effective_addr(8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    //return value of dvmAllocObject is in %eax
    //if return value is null, throw exception
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true);
    conditional_jump(Condition_NE, ".new_instance_done", true);
    //jump to dvmJitToExceptionThrown
    scratchRegs[0] = PhysicalReg_SCRATCH_4;
    jumpToExceptionThrown(3/*exception number*/);
    insertLabel(".new_instance_done", true);
    set_virtual_reg(vA, OpndSize_32, PhysicalReg_EAX, true);
    rPC += 2;
    return 0;
}

//! function to initialize a class

//!INPUT: %eax (class object) %eax is recovered before return
//!OUTPUT: none
//!CALL: dvmInitClass
//!%eax, %esi, %ebx are live through function new_instance_needinit
int new_instance_needinit() {
    insertLabel(".new_instance_needinit", false);
    load_effective_addr(-8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, PhysicalReg_EAX, true, 0, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, PhysicalReg_EAX, true, 4, PhysicalReg_ESP, true);
    scratchRegs[0] = PhysicalReg_ECX;
    call_dvmInitClass();
    //if return value of dvmInitClass is zero, throw exception
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true);
    //recover EAX with the class object
    move_mem_to_reg(OpndSize_32, 4, PhysicalReg_ESP, true, PhysicalReg_EAX, true);
    load_effective_addr(8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    conditional_jump(Condition_E, "common_exceptionThrown", false);
    x86_return();
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_GPR_3

#define P_GPR_1 PhysicalReg_EBX //live through C function, must in callee-saved reg
#define P_GPR_2 PhysicalReg_ECX
#define P_GPR_3 PhysicalReg_EDX
//! lower bytecode NEW_ARRAY

//! It will use helper function if switch is on
int op_new_array() {
    u4 tmp = (u4)FETCH(1);
    u2 vA = INST_A(inst); //destination
    u2 vB = INST_B(inst); //length
    /////////////////////////
    //   REGS used: %esi, %eax, P_GPR_1, P_GPR_2
    //   CALL class_resolve, dvmAllocArrayByClass
    export_pc(); //use %edx
    //check size of the array, if negative, throw exception
    get_virtual_reg(vB, OpndSize_32, 5, false);
    compare_imm_reg(OpndSize_32, 0, 5, false);
    handlePotentialException(Condition_S, Condition_NS,
                             1, "common_errNegArraySize");
    void *classPtr = (void*)
        (currentMethod->clazz->pDvmDex->pResClasses[tmp]);
    assert(classPtr != NULL);
    //here, class is already resolved, the class object is in %eax
    //prepare to call dvmAllocArrayByClass with inputs: resolved class, array length, flag ALLOC_DONT_TRACK
    insertLabel(".new_array_resolved", true);
    load_effective_addr(-12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    /* 1st argument to dvmAllocArrayByClass at 0(%esp) */
    move_imm_to_mem(OpndSize_32, (int)classPtr, 0, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, 5, false, 4, PhysicalReg_ESP, true);
    move_imm_to_mem(OpndSize_32, ALLOC_DONT_TRACK, 8, PhysicalReg_ESP, true);
    scratchRegs[0] = PhysicalReg_SCRATCH_3;
    nextVersionOfHardReg(PhysicalReg_EAX, 3); //next version has 3 refs
    call_dvmAllocArrayByClass();
    load_effective_addr(12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);

    //the allocated object is in %eax
    //check whether it is null, throw exception if null
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true);
    conditional_jump(Condition_NE, ".new_array_done", true);
    //jump to dvmJitToExceptionThrown
    scratchRegs[0] = PhysicalReg_SCRATCH_4;
    jumpToExceptionThrown(2/*exception number*/);
    insertLabel(".new_array_done", true);
    set_virtual_reg(vA, OpndSize_32, PhysicalReg_EAX, true);
    //////////////////////////////////////
    rPC += 2;
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_GPR_3

#define P_GPR_1 PhysicalReg_EBX
#define P_GPR_2 PhysicalReg_ECX
#define P_GPR_3 PhysicalReg_ESI
//! common code to lower FILLED_NEW_ARRAY

//! call: class_resolve call_dvmAllocPrimitiveArray
//! exception: filled_new_array_notimpl common_exceptionThrown
int common_filled_new_array(u2 length, u4 tmp, bool hasRange) {
    ClassObject *classPtr =
              (currentMethod->clazz->pDvmDex->pResClasses[tmp]);
    if(classPtr != NULL) ALOGI("FILLED_NEW_ARRAY class %s", classPtr->descriptor);
    //check whether class is resolved, if yes, jump to resolved
    //if not, call class_resolve
    scratchRegs[0] = PhysicalReg_SCRATCH_1; scratchRegs[1] = PhysicalReg_SCRATCH_2;
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;
    get_res_classes(3, false);
    move_mem_to_reg(OpndSize_32, tmp*4, 3, false, PhysicalReg_EAX, true);
    export_pc();
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true); //resolved class
    conditional_jump(Condition_NE, ".filled_new_array_resolved", true);
    rememberState(1);
    move_imm_to_reg(OpndSize_32, tmp, PhysicalReg_EAX, true);
    call_helper_API(".class_resolve");
    transferToState(1);
    //here, class is already resolved
    insertLabel(".filled_new_array_resolved", true);
    //check descriptor of the class object, if not implemented, throws exception
    move_mem_to_reg(OpndSize_32, 24, PhysicalReg_EAX, true, 5, false);
    //load a single byte of the descriptor
    movez_mem_to_reg(OpndSize_8, 1, 5, false, 6, false);
    compare_imm_reg(OpndSize_32, 'I', 6, false);
    conditional_jump(Condition_E, ".filled_new_array_impl", true);
    compare_imm_reg(OpndSize_32, 'L', 6, false);
    conditional_jump(Condition_E, ".filled_new_array_impl", true);
    compare_imm_reg(OpndSize_32, '[', 6, false);
    conditional_jump(Condition_NE, ".filled_new_array_notimpl", false);

    insertLabel(".filled_new_array_impl", true);
    //prepare to call dvmAllocArrayByClass with inputs: classObject, length, flag ALLOC_DONT_TRACK
    load_effective_addr(-12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_imm_to_mem(OpndSize_32, (int)classPtr, 0, PhysicalReg_ESP, true);
    move_imm_to_mem(OpndSize_32, length, 4, PhysicalReg_ESP, true);
    move_imm_to_mem(OpndSize_32, ALLOC_DONT_TRACK, 8, PhysicalReg_ESP, true);
    scratchRegs[0] = PhysicalReg_SCRATCH_3; scratchRegs[1] = PhysicalReg_Null;
    if(hasRange) {
        nextVersionOfHardReg(PhysicalReg_EAX, 5+(length >= 1 ? LOOP_COUNT : 0)); //next version
    }
    else {
        nextVersionOfHardReg(PhysicalReg_EAX, 5+length); //next version
    }
    call_dvmAllocArrayByClass();
    load_effective_addr(12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    //return value of dvmAllocPrimitiveArray is in %eax
    //if the return value is null, throw exception
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true);
    handlePotentialException(
                                       Condition_E, Condition_NE,
                                       3, "common_exceptionThrown");

    /* we need to mark the card of the new array, if it's not an int */
    compare_imm_reg(OpndSize_32, 'I', 6, false);
    conditional_jump(Condition_E, ".dont_mark_filled_new_array", true);

    // Need to make copy of EAX, because it's used later in op_filled_new_array()
    move_reg_to_reg(OpndSize_32, PhysicalReg_EAX, true, 6, false);

    markCard_filled(6, false, PhysicalReg_SCRATCH_4, false);

    insertLabel(".dont_mark_filled_new_array", true);

    //return value of bytecode FILLED_NEW_ARRAY is in GLUE structure
    scratchRegs[0] = PhysicalReg_SCRATCH_4; scratchRegs[1] = PhysicalReg_Null;
    set_return_value(OpndSize_32, PhysicalReg_EAX, true);
    return 0;
}
//! LOWER bytecode FILLED_NEW_ARRAY

//!
int op_filled_new_array() {
    u2 length = INST_B(inst);
    u4 tmp = (u4)FETCH(1);
    u2 v5 = INST_A(inst);
    u2 vv = FETCH(2);
    u2 v1 = vv & 0xf;
    u2 v2 = (vv >> 4) & 0xf;
    u2 v3 = (vv >> 8) & 0xf;
    u2 v4 = (vv >> 12) & 0xf;
    common_filled_new_array(length, tmp, false);
    if(length >= 1) {
        //move from virtual register to contents of array object
        get_virtual_reg(v1, OpndSize_32, 7, false);
        move_reg_to_mem(OpndSize_32, 7, false, offArrayObject_contents, PhysicalReg_EAX, true);
    }
    if(length >= 2) {
        //move from virtual register to contents of array object
        get_virtual_reg(v2, OpndSize_32, 8, false);
        move_reg_to_mem(OpndSize_32, 8, false, offArrayObject_contents+4, PhysicalReg_EAX, true);
    }
    if(length >= 3) {
        //move from virtual register to contents of array object
        get_virtual_reg(v3, OpndSize_32, 9, false);
        move_reg_to_mem(OpndSize_32, 9, false, offArrayObject_contents+8, PhysicalReg_EAX, true);
    }
    if(length >= 4) {
        //move from virtual register to contents of array object
        get_virtual_reg(v4, OpndSize_32, 10, false);
        move_reg_to_mem(OpndSize_32, 10, false, offArrayObject_contents+12, PhysicalReg_EAX, true);
    }
    if(length >= 5) {
        //move from virtual register to contents of array object
        get_virtual_reg(v5, OpndSize_32, 11, false);
        move_reg_to_mem(OpndSize_32, 11, false, offArrayObject_contents+16, PhysicalReg_EAX, true);
    }
    rPC += 3;
    return 0;
}
//! function to handle the error of array not implemented

//!
int filled_new_array_notimpl() {
    //two inputs for common_throw:
    insertLabel(".filled_new_array_notimpl", false);
    move_imm_to_reg(OpndSize_32, LstrFilledNewArrayNotImpl, PhysicalReg_EAX, true);
    move_imm_to_reg(OpndSize_32, (int) gDvm.exInternalError, PhysicalReg_ECX, true);
    unconditional_jump("common_throw", false);
    return 0;
}

#define P_SCRATCH_1 PhysicalReg_EDX
//! LOWER bytecode FILLED_NEW_ARRAY_RANGE

//!
int op_filled_new_array_range() {
    u2 length = INST_AA(inst);
    u4 tmp = (u4)FETCH(1);
    u4 vC = (u4)FETCH(2);
    common_filled_new_array(length, tmp, true/*hasRange*/);
    //here, %eax points to the array object
    if(length >= 1) {
        //dump all virtual registers used by this bytecode to stack, for NCG O1
        int k;
        for(k = 0; k < length; k++) {
            spillVirtualReg(vC+k, LowOpndRegType_gp, true); //will update refCount
        }
        //address of the first virtual register that will be moved to the array object
        load_effective_addr(vC*4, PhysicalReg_FP, true, 7, false); //addr
        //start address for contents of the array object
        load_effective_addr(offArrayObject_contents, PhysicalReg_EAX, true, 8, false); //addr
        //loop counter
        move_imm_to_reg(OpndSize_32, length-1, 9, false); //counter
        //start of the loop
        insertLabel(".filled_new_array_range_loop1", true);
        rememberState(1);
        move_mem_to_reg(OpndSize_32, 0, 7, false, 10, false);
        load_effective_addr(4, 7, false, 7, false);
        move_reg_to_mem(OpndSize_32, 10, false, 0, 8, false);
        load_effective_addr(4, 8, false, 8, false);
        alu_binary_imm_reg(OpndSize_32, sub_opc, 1, 9, false);
        transferToState(1);
        //jump back to the loop start
        conditional_jump(Condition_NS, ".filled_new_array_range_loop1", true);
    }
    rPC += 3;
    return 0;
}
#undef P_GPR_1
#undef P_GPR_2
#undef P_GPR_3
#undef P_SCRATCH_1

#define P_GPR_1 PhysicalReg_EBX
//! LOWER bytecode FILL_ARRAY_DATA

//!use 1 GPR and scratch regs (export_pc dvmInterpHandleFillArrayData)
//!CALL: dvmInterpHandleFillArrayData
int op_fill_array_data() {
    u2 vA = INST_AA(inst);
    u4 tmp = (u4)FETCH(1);
    tmp |= (u4)FETCH(2) << 16;
    scratchRegs[0] = PhysicalReg_SCRATCH_1;
    scratchRegs[1] = PhysicalReg_Null;
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;
    get_virtual_reg(vA, OpndSize_32, 1, false);
    //prepare to call dvmInterpHandleFillArrayData, input: array object, address of the data
    load_effective_addr(-8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, 1, false, 0, PhysicalReg_ESP, true);
    /* 2nd argument to dvmInterpHandleFillArrayData at 4(%esp) */
    move_imm_to_mem(OpndSize_32, (int)(rPC+tmp), 4, PhysicalReg_ESP, true);
    call_dvmInterpHandleFillArrayData();
    load_effective_addr(8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);

    //check return value of dvmInterpHandleFillArrayData, if zero, throw exception
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true);
    conditional_jump(Condition_NE, ".fill_array_data_done", true);
    //jump to dvmJitToExceptionThrown
    scratchRegs[0] = PhysicalReg_SCRATCH_2;
    jumpToExceptionThrown(2/*exception number*/);
    insertLabel(".fill_array_data_done", true);
    rPC += 3;
    return 0;
}
#undef P_GPR_1

#define P_GPR_1 PhysicalReg_EBX
//! LOWER bytecode THROW

//!
int op_throw() {
    u2 vA = INST_AA(inst);
    export_pc();
    get_virtual_reg(vA, OpndSize_32, 1, false);
    //null check
    compare_imm_reg(OpndSize_32, 0, 1, false);
    conditional_jump(Condition_E, "common_errNullObject", false);
    //set glue->exception & throw exception
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;
    scratchRegs[0] = PhysicalReg_SCRATCH_1; scratchRegs[1] = PhysicalReg_SCRATCH_2;
    set_exception(1, false);
    unconditional_jump("common_exceptionThrown", false);
    rPC += 1;
    return 0;
}
#undef P_GPR_1
#define P_GPR_1 PhysicalReg_EBX
//! LOWER bytecode THROW_VERIFICATION_ERROR

//! op AA, ref@BBBB
int op_throw_verification_error() {
    u2 vA, vB;
    vA = INST_AA(inst);
    vB = FETCH(1);

    export_pc();
    scratchRegs[0] = PhysicalReg_SCRATCH_1;
    get_glue_method(1, false);

    load_effective_addr(-12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_imm_to_mem(OpndSize_32, vB, 8, PhysicalReg_ESP, true);
    move_imm_to_mem(OpndSize_32, vA, 4, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, 1, false, 0, PhysicalReg_ESP, true);
    scratchRegs[0] = PhysicalReg_SCRATCH_2;
    call_dvmThrowVerificationError();
    load_effective_addr(12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);

    unconditional_jump("common_exceptionThrown", false);
    rPC += 2;
    return 0;
}
#undef P_GPR_1
