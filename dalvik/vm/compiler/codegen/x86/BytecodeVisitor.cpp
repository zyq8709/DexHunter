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


/*! \file BytecodeVisitor.cpp
    \brief This file implements visitors of the bytecode
*/
#include "libdex/DexOpcodes.h"
#include "libdex/DexFile.h"
#include "Lower.h"
#include "AnalysisO1.h"

//! Returns size of the current bytecode in u2 unit

//!
int getByteCodeSize() { //uses inst, unit in u2
    switch (INST_INST(inst)) {
    case OP_NOP:
        return 1;
    case OP_MOVE:
    case OP_MOVE_OBJECT:
        return 1;
    case OP_MOVE_FROM16:
    case OP_MOVE_OBJECT_FROM16:
        return 2;
    case OP_MOVE_16:
    case OP_MOVE_OBJECT_16:
        return 3;
    case OP_MOVE_WIDE:
        return 1;
    case OP_MOVE_WIDE_FROM16:
        return 2;
    case OP_MOVE_WIDE_16:
        return 3;
    case OP_MOVE_RESULT:
    case OP_MOVE_RESULT_OBJECT:
        return 1;
    case OP_MOVE_RESULT_WIDE:
        return 1;
    case OP_MOVE_EXCEPTION:
        return 1;
    case OP_RETURN_VOID:
    case OP_RETURN_VOID_BARRIER:
        return 1;
    case OP_RETURN:
    case OP_RETURN_OBJECT:
        return 1;
    case OP_RETURN_WIDE:
        return 1;
    case OP_CONST_4:
        return 1;
    case OP_CONST_16:
        return 2;
    case OP_CONST:
        return 3;
    case OP_CONST_HIGH16:
        return 2;
    case OP_CONST_WIDE_16:
        return 2;
    case OP_CONST_WIDE_32:
        return 3;
    case OP_CONST_WIDE:
        return 5;
    case OP_CONST_WIDE_HIGH16:
        return 2;
    case OP_CONST_STRING:
        return 2;
    case OP_CONST_STRING_JUMBO:
        return 3;
    case OP_CONST_CLASS:
        return 2;
    case OP_MONITOR_ENTER:
        return 1;
    case OP_MONITOR_EXIT:
        return 1;
    case OP_CHECK_CAST:
        return 2;
    case OP_INSTANCE_OF:
        return 2;
    case OP_ARRAY_LENGTH:
        return 1;
    case OP_NEW_INSTANCE:
        return 2;
    case OP_NEW_ARRAY:
        return 2;
    case OP_FILLED_NEW_ARRAY:
        return 3;
    case OP_FILLED_NEW_ARRAY_RANGE:
        return 3;
    case OP_FILL_ARRAY_DATA:
        return 3;
    case OP_THROW:
        return 1;
    case OP_THROW_VERIFICATION_ERROR:
        return 2;
    case OP_GOTO:
        return 1;
    case OP_GOTO_16:
        return 2;
    case OP_GOTO_32:
        return 3;
    case OP_PACKED_SWITCH:
        return 3;
    case OP_SPARSE_SWITCH:
        return 3;
    case OP_CMPL_FLOAT:
        return 2;
    case OP_CMPG_FLOAT:
        return 2;
    case OP_CMPL_DOUBLE:
        return 2;
    case OP_CMPG_DOUBLE:
        return 2;
    case OP_CMP_LONG:
        return 2;
    case OP_IF_EQ:
        return 2;
    case OP_IF_NE:
        return 2;
    case OP_IF_LT:
        return 2;
    case OP_IF_GE:
        return 2;
    case OP_IF_GT:
        return 2;
    case OP_IF_LE:
        return 2;
    case OP_IF_EQZ:
        return 2;
    case OP_IF_NEZ:
        return 2;
    case OP_IF_LTZ:
        return 2;
    case OP_IF_GEZ:
        return 2;
    case OP_IF_GTZ:
        return 2;
    case OP_IF_LEZ:
        return 2;
    case OP_AGET:
        return 2;
    case OP_AGET_WIDE:
        return 2;
    case OP_AGET_OBJECT:
        return 2;
    case OP_AGET_BOOLEAN:
        return 2;
    case OP_AGET_BYTE:
        return 2;
    case OP_AGET_CHAR:
        return 2;
    case OP_AGET_SHORT:
        return 2;
    case OP_APUT:
        return 2;
    case OP_APUT_WIDE:
        return 2;
    case OP_APUT_OBJECT:
        return 2;
    case OP_APUT_BOOLEAN:
        return 2;
    case OP_APUT_BYTE:
        return 2;
    case OP_APUT_CHAR:
        return 2;
    case OP_APUT_SHORT:
        return 2;
    case OP_IGET:
    case OP_IGET_WIDE:
    case OP_IGET_OBJECT:
    case OP_IGET_VOLATILE:
    case OP_IGET_WIDE_VOLATILE:
    case OP_IGET_OBJECT_VOLATILE:
    case OP_IGET_BOOLEAN:
    case OP_IGET_BYTE:
    case OP_IGET_CHAR:
    case OP_IGET_SHORT:
    case OP_IPUT:
    case OP_IPUT_WIDE:
    case OP_IPUT_OBJECT:
    case OP_IPUT_VOLATILE:
    case OP_IPUT_WIDE_VOLATILE:
    case OP_IPUT_OBJECT_VOLATILE:
    case OP_IPUT_BOOLEAN:
    case OP_IPUT_BYTE:
    case OP_IPUT_CHAR:
    case OP_IPUT_SHORT:
        return 2;
    case OP_SGET:
    case OP_SGET_WIDE:
    case OP_SGET_OBJECT:
    case OP_SGET_VOLATILE:
    case OP_SGET_WIDE_VOLATILE:
    case OP_SGET_OBJECT_VOLATILE:
    case OP_SGET_BOOLEAN:
    case OP_SGET_BYTE:
    case OP_SGET_CHAR:
    case OP_SGET_SHORT:
    case OP_SPUT:
    case OP_SPUT_WIDE:
    case OP_SPUT_OBJECT:
    case OP_SPUT_VOLATILE:
    case OP_SPUT_WIDE_VOLATILE:
    case OP_SPUT_OBJECT_VOLATILE:
    case OP_SPUT_BOOLEAN:
    case OP_SPUT_BYTE:
    case OP_SPUT_CHAR:
    case OP_SPUT_SHORT:
        return 2;
    case OP_INVOKE_VIRTUAL:
    case OP_INVOKE_SUPER:
    case OP_INVOKE_DIRECT:
    case OP_INVOKE_STATIC:
    case OP_INVOKE_INTERFACE:
    case OP_INVOKE_VIRTUAL_RANGE:
    case OP_INVOKE_SUPER_RANGE:
    case OP_INVOKE_DIRECT_RANGE:
    case OP_INVOKE_STATIC_RANGE:
    case OP_INVOKE_INTERFACE_RANGE:
        return 3;

    case OP_NEG_INT:
    case OP_NOT_INT:
    case OP_NEG_LONG:
    case OP_NOT_LONG:
    case OP_NEG_FLOAT:
    case OP_NEG_DOUBLE:
    case OP_INT_TO_LONG:
    case OP_INT_TO_FLOAT:
    case OP_INT_TO_DOUBLE:
    case OP_LONG_TO_INT:
    case OP_LONG_TO_FLOAT:
    case OP_LONG_TO_DOUBLE:
    case OP_FLOAT_TO_INT:
    case OP_FLOAT_TO_LONG:
    case OP_FLOAT_TO_DOUBLE:
    case OP_DOUBLE_TO_INT:
    case OP_DOUBLE_TO_LONG:
    case OP_DOUBLE_TO_FLOAT:
    case OP_INT_TO_BYTE:
    case OP_INT_TO_CHAR:
    case OP_INT_TO_SHORT:
        return 1;

    case OP_ADD_INT:
    case OP_SUB_INT:
    case OP_MUL_INT:
    case OP_DIV_INT:
    case OP_REM_INT:
    case OP_AND_INT:
    case OP_OR_INT:
    case OP_XOR_INT:
    case OP_SHL_INT:
    case OP_SHR_INT:
    case OP_USHR_INT:
    case OP_ADD_LONG:
    case OP_SUB_LONG:
    case OP_MUL_LONG:
    case OP_DIV_LONG:
    case OP_REM_LONG:
    case OP_AND_LONG:
    case OP_OR_LONG:
    case OP_XOR_LONG:
    case OP_SHL_LONG:
    case OP_SHR_LONG:
    case OP_USHR_LONG:
    case OP_ADD_FLOAT:
    case OP_SUB_FLOAT:
    case OP_MUL_FLOAT:
    case OP_DIV_FLOAT:
    case OP_REM_FLOAT:
    case OP_ADD_DOUBLE:
    case OP_SUB_DOUBLE:
    case OP_MUL_DOUBLE:
    case OP_DIV_DOUBLE:
    case OP_REM_DOUBLE:
        return 2;

    case OP_ADD_INT_2ADDR:
    case OP_SUB_INT_2ADDR:
    case OP_MUL_INT_2ADDR:
    case OP_DIV_INT_2ADDR:
    case OP_REM_INT_2ADDR:
    case OP_AND_INT_2ADDR:
    case OP_OR_INT_2ADDR:
    case OP_XOR_INT_2ADDR:
    case OP_SHL_INT_2ADDR:
    case OP_SHR_INT_2ADDR:
    case OP_USHR_INT_2ADDR:
    case OP_ADD_LONG_2ADDR:
    case OP_SUB_LONG_2ADDR:
    case OP_MUL_LONG_2ADDR:
    case OP_DIV_LONG_2ADDR:
    case OP_REM_LONG_2ADDR:
    case OP_AND_LONG_2ADDR:
    case OP_OR_LONG_2ADDR:
    case OP_XOR_LONG_2ADDR:
    case OP_SHL_LONG_2ADDR:
    case OP_SHR_LONG_2ADDR:
    case OP_USHR_LONG_2ADDR:
    case OP_ADD_FLOAT_2ADDR:
    case OP_SUB_FLOAT_2ADDR:
    case OP_MUL_FLOAT_2ADDR:
    case OP_DIV_FLOAT_2ADDR:
    case OP_REM_FLOAT_2ADDR:
    case OP_ADD_DOUBLE_2ADDR:
    case OP_SUB_DOUBLE_2ADDR:
    case OP_MUL_DOUBLE_2ADDR:
    case OP_DIV_DOUBLE_2ADDR:
    case OP_REM_DOUBLE_2ADDR:
        return 1;

    case OP_ADD_INT_LIT16:
    case OP_RSUB_INT:
    case OP_MUL_INT_LIT16:
    case OP_DIV_INT_LIT16:
    case OP_REM_INT_LIT16:
    case OP_AND_INT_LIT16:
    case OP_OR_INT_LIT16:
    case OP_XOR_INT_LIT16:
        return 2;

    case OP_ADD_INT_LIT8:
    case OP_RSUB_INT_LIT8:
    case OP_MUL_INT_LIT8:
    case OP_DIV_INT_LIT8:
    case OP_REM_INT_LIT8:
    case OP_AND_INT_LIT8:
    case OP_OR_INT_LIT8:
    case OP_XOR_INT_LIT8:
    case OP_SHL_INT_LIT8:
    case OP_SHR_INT_LIT8:
    case OP_USHR_INT_LIT8:
        return 2;

    case OP_EXECUTE_INLINE:
    case OP_EXECUTE_INLINE_RANGE:
        return 3;
#if FIXME
    case OP_INVOKE_OBJECT_INIT_RANGE:
        return 3;
#endif

    case OP_IGET_QUICK:
    case OP_IGET_WIDE_QUICK:
    case OP_IGET_OBJECT_QUICK:
    case OP_IPUT_QUICK:
    case OP_IPUT_WIDE_QUICK:
    case OP_IPUT_OBJECT_QUICK:
        return 2;

    case OP_INVOKE_VIRTUAL_QUICK:
    case OP_INVOKE_VIRTUAL_QUICK_RANGE:
    case OP_INVOKE_SUPER_QUICK:
    case OP_INVOKE_SUPER_QUICK_RANGE:
        return 3;
#ifdef SUPPORT_HLO
    case kExtInstruction:
        switch(inst) {
        case OP_X_AGET_QUICK:
        case OP_X_AGET_WIDE_QUICK:
        case OP_X_AGET_OBJECT_QUICK:
    case OP_X_AGET_BOOLEAN_QUICK:
    case OP_X_AGET_BYTE_QUICK:
    case OP_X_AGET_CHAR_QUICK:
    case OP_X_AGET_SHORT_QUICK:
    case OP_X_APUT_QUICK:
    case OP_X_APUT_WIDE_QUICK:
    case OP_X_APUT_OBJECT_QUICK:
    case OP_X_APUT_BOOLEAN_QUICK:
    case OP_X_APUT_BYTE_QUICK:
    case OP_X_APUT_CHAR_QUICK:
    case OP_X_APUT_SHORT_QUICK:
        return 3;
    case OP_X_DEREF_GET:
    case OP_X_DEREF_GET_OBJECT:
    case OP_X_DEREF_GET_WIDE:
    case OP_X_DEREF_GET_BOOLEAN:
    case OP_X_DEREF_GET_BYTE:
    case OP_X_DEREF_GET_CHAR:
    case OP_X_DEREF_GET_SHORT:
    case OP_X_DEREF_PUT:
    case OP_X_DEREF_PUT_WIDE:
    case OP_X_DEREF_PUT_OBJECT:
    case OP_X_DEREF_PUT_BOOLEAN:
    case OP_X_DEREF_PUT_BYTE:
    case OP_X_DEREF_PUT_CHAR:
    case OP_X_DEREF_PUT_SHORT:
        return 2;
    case OP_X_ARRAY_CHECKS:
    case OP_X_ARRAY_OBJECT_CHECKS:
        return 3;
    case OP_X_CHECK_BOUNDS:
    case OP_X_CHECK_NULL:
    case OP_X_CHECK_TYPE:
        return 2;
    }
#endif
    }
    return -1;
}
//! reduces refCount of a virtual register

//!
void touchOneVR(u2 vA, LowOpndRegType type) {
    int index = searchCompileTable(LowOpndRegType_virtual | type, vA);
    if(index < 0) {
        ALOGE("virtual reg %d type %d not found in touchOneVR", vA, type);
        return;
    }
    compileTable[index].refCount--;
}
//! reduces refCount of two virtual registers

//!
void touchTwoVRs(u2 vA, u2 vB, LowOpndRegType type) {
    int index = searchCompileTable(LowOpndRegType_virtual | type, vA);
    if(index < 0) {
        ALOGE("virtual reg vA %d type %d not found in touchTwoVRs", vA, type);
        return;
    }
    compileTable[index].refCount--;
    index = searchCompileTable(LowOpndRegType_virtual | type, vB);
    if(index < 0) {
        ALOGE("virtual reg vB %d type %d not found in touchTwoVRs", vB, type);
        return;
    }
    compileTable[index].refCount--;
}
int num_const_worklist;
//! worklist to update constVRTable later
int constWorklist[10];

int num_const_vr; //in a basic block
//! table to store the constant information for virtual registers
ConstVRInfo constVRTable[MAX_CONST_REG];
//! update constVRTable for a given virtual register

//! set "isConst" to false
void setVRToNonConst(int regNum, OpndSize size) {
    int k;
    int indexL = -1;
    int indexH = -1;
    for(k = 0; k < num_const_vr; k++) {
        if(constVRTable[k].regNum == regNum) {
            indexL = k;
            continue;
        }
        if(constVRTable[k].regNum == regNum + 1 && size == OpndSize_64) {
            indexH = k;
            continue;
        }
    }
    if(indexL >= 0) {
        //remove this entry??
        constVRTable[indexL].isConst = false;
    }
    if(size == OpndSize_64 && indexH >= 0) {
        constVRTable[indexH].isConst = false;
    }
}
//! update constVRTable for a given virtual register

//! set "isConst" to true
void setVRToConst(int regNum, OpndSize size, int* tmpValue) {
    int k;
    int indexL = -1;
    int indexH = -1;
    for(k = 0; k < num_const_vr; k++) {
        if(constVRTable[k].regNum == regNum) {
            indexL = k;
            continue;
        }
        if(constVRTable[k].regNum == regNum + 1 && size == OpndSize_64) {
            indexH = k;
            continue;
        }
    }
    if(indexL < 0) {
        indexL = num_const_vr;
        constVRTable[indexL].regNum = regNum;
        num_const_vr++;
    }
    constVRTable[indexL].isConst = true;
    constVRTable[indexL].value = tmpValue[0];
    if(size == OpndSize_64) {
        if(indexH < 0) {
            indexH = num_const_vr;
            constVRTable[indexH].regNum = regNum+1;
            num_const_vr++;
        }
        constVRTable[indexH].isConst = true;
        constVRTable[indexH].value = tmpValue[1];
    }
    if(num_const_vr > MAX_CONST_REG) ALOGE("constVRTable overflows");
    invalidateVRDueToConst(regNum, size);
}

//! perform work on constWorklist

//!
void updateConstInfo(BasicBlock_O1* bb) {
    if(bb == NULL) return;
    int k;
    for(k = 0; k < num_const_worklist; k++) {
        //int indexOrig = constWorklist[k];
        //compileTable[indexOrig].isConst = false;
        //int A = compileTable[indexOrig].regNum;
        //LowOpndRegType type = compileTable[indexOrig].physicalType & MASK_FOR_TYPE;
        setVRToNonConst(constWorklist[k], OpndSize_32);
    }
}
//! check whether the current bytecode generates a const

//! if yes, update constVRTable; otherwise, update constWorklist
//! if a bytecode uses vA (const), and updates vA to non const, getConstInfo will return false and update constWorklist to make sure when lowering the bytecode, vA is treated as constant
bool getConstInfo(BasicBlock_O1* bb) {
    compileTableEntry* infoArray = compileTable;
    u2 inst_op = INST_INST(inst);
    u2 vA = 0, vB = 0, v1, v2;
    u2 BBBB;
    u2 tmp_u2;
    s4 tmp_s4;
    u4 tmp_u4;
    int entry, tmpValue[2], tmpValue2[2];
    num_const_worklist = 0;

    switch(inst_op) {
        //for other opcode, if update the register, set isConst to false
    case OP_MOVE:
    case OP_MOVE_OBJECT:
    case OP_MOVE_FROM16:
    case OP_MOVE_OBJECT_FROM16:
    case OP_MOVE_16:
    case OP_MOVE_OBJECT_16:
        if(inst_op == OP_MOVE || inst_op == OP_MOVE_OBJECT) {
            vA = INST_A(inst);
            vB = INST_B(inst);
        }
        else if(inst_op == OP_MOVE_FROM16 || inst_op == OP_MOVE_OBJECT_FROM16) {
            vA = INST_AA(inst);
            vB = FETCH(1);
        }
        else if(inst_op == OP_MOVE_16 || inst_op == OP_MOVE_OBJECT_16) {
            vA = FETCH(1);
            vB = FETCH(2);
        }
        if(isVirtualRegConstant(vB, LowOpndRegType_gp, tmpValue, false) == 3) {
            entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
            setVRToConst(vA, OpndSize_32, tmpValue);
            infoArray[entry].isConst = true;
            infoArray[entry].value[0] = tmpValue[0];
            compileTable[entry].refCount--;
            touchOneVR(vB, LowOpndRegType_gp);
            return true;
        } else {
            constWorklist[num_const_worklist] = vA;
            num_const_worklist++;
        }
        return false;
    case OP_MOVE_WIDE:
    case OP_MOVE_WIDE_FROM16:
    case OP_MOVE_WIDE_16:
        if(inst_op == OP_MOVE_WIDE) {
            vA = INST_A(inst);
            vB = INST_B(inst);
        }
        else if(inst_op == OP_MOVE_WIDE_FROM16) {
            vA = INST_AA(inst);
            vB = FETCH(1);
        }
        else if(inst_op == OP_MOVE_WIDE_16) {
            vA = FETCH(1);
            vB = FETCH(2);
        }
        if(isVirtualRegConstant(vB, LowOpndRegType_xmm, tmpValue, false) == 3) {
            entry = findVirtualRegInTable(vA, LowOpndRegType_xmm, true);
            setVRToConst(vA, OpndSize_64, tmpValue);
            compileTable[entry].refCount--;
            touchOneVR(vB, LowOpndRegType_xmm);
            return true;
        } else {
            constWorklist[num_const_worklist] = vA;
            num_const_worklist++;
            constWorklist[num_const_worklist] = vA+1;
            num_const_worklist++;
        }
        return false;
    case OP_MOVE_RESULT:
    case OP_MOVE_RESULT_OBJECT:
    case OP_MOVE_EXCEPTION:
    case OP_CONST_STRING:
    case OP_CONST_STRING_JUMBO:
    case OP_CONST_CLASS:
    case OP_NEW_INSTANCE:
    case OP_CMPL_FLOAT:
    case OP_CMPG_FLOAT:
    case OP_CMPL_DOUBLE:
    case OP_CMPG_DOUBLE:
    case OP_AGET:
    case OP_AGET_OBJECT:
    case OP_AGET_BOOLEAN:
    case OP_AGET_BYTE:
    case OP_AGET_CHAR:
    case OP_AGET_SHORT:
    case OP_SGET:
    case OP_SGET_OBJECT:
    case OP_SGET_VOLATILE:
    case OP_SGET_OBJECT_VOLATILE:
    case OP_SGET_BOOLEAN:
    case OP_SGET_BYTE:
    case OP_SGET_CHAR:
    case OP_SGET_SHORT:
        vA = INST_AA(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        return false;
    case OP_MOVE_RESULT_WIDE:
    case OP_AGET_WIDE:
    case OP_SGET_WIDE:
    case OP_SGET_WIDE_VOLATILE:
        vA = INST_AA(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        constWorklist[num_const_worklist] = vA+1;
        num_const_worklist++;
        return false;
    case OP_INSTANCE_OF:
    case OP_ARRAY_LENGTH:
    case OP_NEW_ARRAY:
    case OP_IGET:
    case OP_IGET_OBJECT:
    case OP_IGET_VOLATILE:
    case OP_IGET_OBJECT_VOLATILE:
    case OP_IGET_BOOLEAN:
    case OP_IGET_BYTE:
    case OP_IGET_CHAR:
    case OP_IGET_SHORT:
    case OP_IGET_QUICK:
    case OP_IGET_OBJECT_QUICK:
        vA = INST_A(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        return false;
    case OP_IGET_WIDE:
    case OP_IGET_WIDE_VOLATILE:
    case OP_IGET_WIDE_QUICK:
        vA = INST_A(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        constWorklist[num_const_worklist] = vA+1;
        num_const_worklist++;
        return false;
        //TODO: constant folding for float/double/long ALU
    case OP_ADD_FLOAT:
    case OP_SUB_FLOAT:
    case OP_MUL_FLOAT:
    case OP_DIV_FLOAT:
    case OP_REM_FLOAT:
        vA = INST_AA(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        return false;
    case OP_ADD_DOUBLE:
    case OP_SUB_DOUBLE:
    case OP_MUL_DOUBLE:
    case OP_DIV_DOUBLE:
    case OP_REM_DOUBLE:
        vA = INST_AA(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        constWorklist[num_const_worklist] = vA+1;
        num_const_worklist++;
        return false;
    case OP_NEG_FLOAT:
    case OP_INT_TO_FLOAT:
    case OP_LONG_TO_FLOAT:
    case OP_FLOAT_TO_INT:
    case OP_DOUBLE_TO_INT:
    case OP_ADD_FLOAT_2ADDR:
    case OP_SUB_FLOAT_2ADDR:
    case OP_MUL_FLOAT_2ADDR:
    case OP_DIV_FLOAT_2ADDR:
    case OP_REM_FLOAT_2ADDR:
    case OP_DOUBLE_TO_FLOAT:
        vA = INST_A(inst);
        constWorklist[num_const_worklist] = vA; //change constWorklist to point to vA TODO
        num_const_worklist++;
        return false;
    case OP_FLOAT_TO_LONG:
    case OP_DOUBLE_TO_LONG:
    case OP_FLOAT_TO_DOUBLE:
        vA = INST_A(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        constWorklist[num_const_worklist] = vA+1;
        num_const_worklist++;
        return false;
    case OP_NEG_DOUBLE:
    case OP_INT_TO_DOUBLE: //fp stack
    case OP_LONG_TO_DOUBLE:
    case OP_ADD_DOUBLE_2ADDR:
    case OP_SUB_DOUBLE_2ADDR:
    case OP_MUL_DOUBLE_2ADDR:
    case OP_DIV_DOUBLE_2ADDR:
    case OP_REM_DOUBLE_2ADDR:
        //ops on float, double
        vA = INST_A(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        constWorklist[num_const_worklist] = vA+1;
        num_const_worklist++;
        return false;
    case OP_NEG_INT:
    case OP_NOT_INT:
    case OP_LONG_TO_INT:
    case OP_INT_TO_BYTE:
    case OP_INT_TO_CHAR:
    case OP_INT_TO_SHORT:
        vA = INST_A(inst);
        vB = INST_B(inst);
        if(isVirtualRegConstant(vB, LowOpndRegType_gp, tmpValue, false) == 3) {
            entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
            infoArray[entry].isConst = true;
            if(inst_op == OP_NEG_INT)
                infoArray[entry].value[0] = -tmpValue[0];
            if(inst_op == OP_NOT_INT)
                infoArray[entry].value[0] = ~tmpValue[0]; //CHECK
            if(inst_op == OP_LONG_TO_INT)
                infoArray[entry].value[0] = tmpValue[0];
            if(inst_op == OP_INT_TO_BYTE)// sar
                infoArray[entry].value[0] = (tmpValue[0] << 24) >> 24;
            if(inst_op == OP_INT_TO_CHAR) //shr
                infoArray[entry].value[0] = ((unsigned int)(tmpValue[0] << 16)) >> 16;
            if(inst_op == OP_INT_TO_SHORT) //sar
                infoArray[entry].value[0] = (tmpValue[0] << 16) >> 16;
            tmpValue[0] = infoArray[entry].value[0];
            setVRToConst(vA, OpndSize_32, tmpValue);
            compileTable[entry].refCount--;
            touchOneVR(vB, LowOpndRegType_gp);
#ifdef DEBUG_CONST
            LOGD("getConstInfo: set VR %d to %d", vA, infoArray[entry].value[0]);
#endif
            return true;
        }
        else {
            constWorklist[num_const_worklist] = vA;
            num_const_worklist++;
            return false;
        }
    case OP_NEG_LONG:
    case OP_NOT_LONG:
    case OP_INT_TO_LONG:
        vA = INST_A(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        constWorklist[num_const_worklist] = vA+1; //fixed on 10/15/2009
        num_const_worklist++;
        return false;
    case OP_DIV_INT_2ADDR:
    case OP_REM_INT_2ADDR:
    case OP_REM_INT_LIT16:
    case OP_DIV_INT_LIT16:
    case OP_REM_INT_LIT8:
    case OP_DIV_INT_LIT8:
    case OP_DIV_INT:
    case OP_REM_INT:
        if(inst_op == OP_DIV_INT || inst_op == OP_DIV_INT_LIT8 ||
           inst_op == OP_REM_INT || inst_op == OP_REM_INT_LIT8)
            vA = INST_AA(inst);
        else
            vA = INST_A(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        return false;
    case OP_ADD_INT_2ADDR:
    case OP_SUB_INT_2ADDR:
    case OP_MUL_INT_2ADDR:
    case OP_AND_INT_2ADDR:
    case OP_OR_INT_2ADDR:
    case OP_XOR_INT_2ADDR:
    case OP_SHL_INT_2ADDR:
    case OP_SHR_INT_2ADDR:
    case OP_USHR_INT_2ADDR:
        vA = INST_A(inst);
        v2 = INST_B(inst);
        if(isVirtualRegConstant(vA, LowOpndRegType_gp, tmpValue, false) == 3 &&
           isVirtualRegConstant(v2, LowOpndRegType_gp, tmpValue2, false) == 3) {
            entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
            infoArray[entry].isConst = true;
            if(inst_op == OP_ADD_INT_2ADDR)
                infoArray[entry].value[0] = tmpValue[0] + tmpValue2[0];
            if(inst_op == OP_SUB_INT_2ADDR)
                infoArray[entry].value[0] = tmpValue[0] - tmpValue2[0];
            if(inst_op == OP_MUL_INT_2ADDR)
                infoArray[entry].value[0] = tmpValue[0] * tmpValue2[0];
            if(inst_op == OP_DIV_INT_2ADDR)
                infoArray[entry].value[0] = tmpValue[0] / tmpValue2[0];
            if(inst_op == OP_REM_INT_2ADDR)
                infoArray[entry].value[0] = tmpValue[0] % tmpValue2[0];
            if(inst_op == OP_AND_INT_2ADDR)
                infoArray[entry].value[0] = tmpValue[0] & tmpValue2[0];
            if(inst_op == OP_OR_INT_2ADDR)
                infoArray[entry].value[0] = tmpValue[0] | tmpValue2[0];
            if(inst_op == OP_XOR_INT_2ADDR)
                infoArray[entry].value[0] = tmpValue[0] ^ tmpValue2[0];
            if(inst_op == OP_SHL_INT_2ADDR)
                infoArray[entry].value[0] = tmpValue[0] << tmpValue2[0];
            if(inst_op == OP_SHR_INT_2ADDR)
                infoArray[entry].value[0] = tmpValue[0] >> tmpValue2[0];
            if(inst_op == OP_USHR_INT_2ADDR)
                infoArray[entry].value[0] = (unsigned int)tmpValue[0] >> tmpValue2[0];
            tmpValue[0] = infoArray[entry].value[0];
            setVRToConst(vA, OpndSize_32, tmpValue);
            compileTable[entry].refCount--;
            touchOneVR(v2, LowOpndRegType_gp);
#ifdef DEBUG_CONST
            LOGD("getConstInfo: set VR %d to %d", vA, infoArray[entry].value[0]);
#endif
            return true;
        }
        else {
            constWorklist[num_const_worklist] = vA;
            num_const_worklist++;
            return false;
        }
    case OP_ADD_INT_LIT16:
    case OP_RSUB_INT:
    case OP_MUL_INT_LIT16:
    case OP_AND_INT_LIT16:
    case OP_OR_INT_LIT16:
    case OP_XOR_INT_LIT16:
        vA = INST_A(inst);
        vB = INST_B(inst);
        tmp_s4 = (s2)FETCH(1);
        if(isVirtualRegConstant(vB, LowOpndRegType_gp, tmpValue, false) == 3) {
            entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
            infoArray[entry].isConst = true;
            if(inst_op == OP_ADD_INT_LIT16)
                infoArray[entry].value[0] = tmpValue[0] + tmp_s4;
            if(inst_op == OP_RSUB_INT)
                infoArray[entry].value[0] = tmp_s4 - tmpValue[0];
            if(inst_op == OP_MUL_INT_LIT16)
                infoArray[entry].value[0] = tmpValue[0] * tmp_s4;
            if(inst_op == OP_DIV_INT_LIT16)
                infoArray[entry].value[0] = tmpValue[0] / tmp_s4;
            if(inst_op == OP_REM_INT_LIT16)
                infoArray[entry].value[0] = tmpValue[0] % tmp_s4;
            if(inst_op == OP_AND_INT_LIT16)
                infoArray[entry].value[0] = tmpValue[0] & tmp_s4;
            if(inst_op == OP_OR_INT_LIT16)
                infoArray[entry].value[0] = tmpValue[0] | tmp_s4;
            if(inst_op == OP_XOR_INT_LIT16)
                infoArray[entry].value[0] = tmpValue[0] ^ tmp_s4;
            tmpValue[0] = infoArray[entry].value[0];
            setVRToConst(vA, OpndSize_32, tmpValue);
            compileTable[entry].refCount--;
            touchOneVR(vB, LowOpndRegType_gp);
#ifdef DEBUG_CONST
            LOGD("getConstInfo: set VR %d to %d", vA, infoArray[entry].value[0]);
#endif
            return true;
        }
        else {
            constWorklist[num_const_worklist] = vA;
            num_const_worklist++;
            return false;
        }
    case OP_ADD_INT:
    case OP_SUB_INT:
    case OP_MUL_INT:
    case OP_AND_INT:
    case OP_OR_INT:
    case OP_XOR_INT:
    case OP_SHL_INT:
    case OP_SHR_INT:
    case OP_USHR_INT:
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        if(isVirtualRegConstant(v1, LowOpndRegType_gp, tmpValue, false) == 3 &&
           isVirtualRegConstant(v2, LowOpndRegType_gp, tmpValue2, false) == 3) {
            entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
            infoArray[entry].isConst = true;
            if(inst_op == OP_ADD_INT)
                infoArray[entry].value[0] = tmpValue[0] + tmpValue2[0];
            if(inst_op == OP_SUB_INT)
                infoArray[entry].value[0] = tmpValue[0] - tmpValue2[0];
            if(inst_op == OP_MUL_INT)
                infoArray[entry].value[0] = tmpValue[0] * tmpValue2[0];
            if(inst_op == OP_DIV_INT)
                infoArray[entry].value[0] = tmpValue[0] / tmpValue2[0];
            if(inst_op == OP_REM_INT)
                infoArray[entry].value[0] = tmpValue[0] % tmpValue2[0];
            if(inst_op == OP_AND_INT)
                infoArray[entry].value[0] = tmpValue[0] & tmpValue2[0];
            if(inst_op == OP_OR_INT)
                infoArray[entry].value[0] = tmpValue[0] | tmpValue2[0];
            if(inst_op == OP_XOR_INT)
                infoArray[entry].value[0] = tmpValue[0] ^ tmpValue2[0];
            if(inst_op == OP_SHL_INT)
                infoArray[entry].value[0] = tmpValue[0] << tmpValue2[0];
            if(inst_op == OP_SHR_INT)
                infoArray[entry].value[0] = tmpValue[0] >> tmpValue2[0];
            if(inst_op == OP_USHR_INT)
                infoArray[entry].value[0] = (unsigned int)tmpValue[0] >> tmpValue2[0];
            tmpValue[0] = infoArray[entry].value[0];
            setVRToConst(vA, OpndSize_32, tmpValue);
            compileTable[entry].refCount--;
            touchOneVR(v1, LowOpndRegType_gp);
            touchOneVR(v2, LowOpndRegType_gp);
#ifdef DEBUG_CONST
            LOGD("getConstInfo: set VR %d to %d", vA, infoArray[entry].value[0]);
#endif
            return true;
        }
        else {
            constWorklist[num_const_worklist] = vA;
            num_const_worklist++;
            return false;
        }
    case OP_ADD_INT_LIT8: //INST_AA
    case OP_RSUB_INT_LIT8:
    case OP_MUL_INT_LIT8:
    case OP_AND_INT_LIT8:
    case OP_OR_INT_LIT8:
    case OP_XOR_INT_LIT8:
    case OP_SHL_INT_LIT8:
    case OP_SHR_INT_LIT8:
    case OP_USHR_INT_LIT8:
        vA = INST_AA(inst);
        vB = (u2)FETCH(1) & 0xff;
        tmp_s4 = (s2)FETCH(1) >> 8;
        if(isVirtualRegConstant(vB, LowOpndRegType_gp, tmpValue, false) == 3) {
            entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
            infoArray[entry].isConst = true;
            if(inst_op == OP_ADD_INT_LIT8)
                infoArray[entry].value[0] = tmpValue[0] + tmp_s4;
            if(inst_op == OP_RSUB_INT_LIT8)
                infoArray[entry].value[0] = tmp_s4 - tmpValue[0];
            if(inst_op == OP_MUL_INT_LIT8)
                infoArray[entry].value[0] = tmpValue[0] * tmp_s4;
            if(inst_op == OP_DIV_INT_LIT8)
                infoArray[entry].value[0] = tmpValue[0] / tmp_s4;
            if(inst_op == OP_REM_INT_LIT8)
                infoArray[entry].value[0] = tmpValue[0] % tmp_s4;
            if(inst_op == OP_AND_INT_LIT8)
                infoArray[entry].value[0] = tmpValue[0] & tmp_s4;
            if(inst_op == OP_OR_INT_LIT8)
                infoArray[entry].value[0] = tmpValue[0] | tmp_s4;
            if(inst_op == OP_XOR_INT_LIT8)
                infoArray[entry].value[0] = tmpValue[0] ^ tmp_s4;
            if(inst_op == OP_SHL_INT_LIT8)
                infoArray[entry].value[0] = tmpValue[0] << tmp_s4;
            if(inst_op == OP_SHR_INT_LIT8)
                infoArray[entry].value[0] = tmpValue[0] >> tmp_s4;
            if(inst_op == OP_USHR_INT_LIT8)
                infoArray[entry].value[0] = (unsigned int)tmpValue[0] >> tmp_s4;
            tmpValue[0] = infoArray[entry].value[0];
            setVRToConst(vA, OpndSize_32, tmpValue);
            compileTable[entry].refCount--;
            touchOneVR(vB, LowOpndRegType_gp);
#ifdef DEBUG_CONST
            LOGD("getConstInfo: set VR %d to %d", vA, infoArray[entry].value[0]);
#endif
            return true;
        }
        else {
            constWorklist[num_const_worklist] = vA;
            num_const_worklist++;
            return false;
        }
    case OP_ADD_LONG:
    case OP_SUB_LONG:
    case OP_AND_LONG:
    case OP_OR_LONG:
    case OP_XOR_LONG:
    case OP_MUL_LONG:
    case OP_DIV_LONG:
    case OP_REM_LONG:
    case OP_SHL_LONG:
    case OP_SHR_LONG:
    case OP_USHR_LONG:
        //TODO bytecode is not going to update state registers
        //constant folding
        vA = INST_AA(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        constWorklist[num_const_worklist] = vA+1;
        num_const_worklist++;
        return false;
    case OP_CMP_LONG:
        vA = INST_AA(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        return false;
    case OP_ADD_LONG_2ADDR:
    case OP_SUB_LONG_2ADDR:
    case OP_AND_LONG_2ADDR:
    case OP_OR_LONG_2ADDR:
    case OP_XOR_LONG_2ADDR:
    case OP_MUL_LONG_2ADDR:
    case OP_DIV_LONG_2ADDR:
    case OP_REM_LONG_2ADDR:
    case OP_SHL_LONG_2ADDR:
    case OP_SHR_LONG_2ADDR:
    case OP_USHR_LONG_2ADDR:
        vA = INST_A(inst);
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        constWorklist[num_const_worklist] = vA+1;
        num_const_worklist++;
        return false;
    case OP_CONST_4:
        vA = INST_A(inst);
        tmp_s4 = (s4) (INST_B(inst) << 28) >> 28;
        entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
        infoArray[entry].isConst = true;
        infoArray[entry].value[0] = tmp_s4;
        tmpValue[0] = infoArray[entry].value[0];
        setVRToConst(vA, OpndSize_32, tmpValue);
        compileTable[entry].refCount--;
#ifdef DEBUG_CONST
        LOGD("getConstInfo: set VR %d to %d", vA, tmp_s4);
#endif
        return true;
    case OP_CONST_16:
        BBBB = FETCH(1);
        vA = INST_AA(inst);
        entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
        infoArray[entry].isConst = true;
        infoArray[entry].value[0] = (s2)BBBB;
        tmpValue[0] = infoArray[entry].value[0];
        setVRToConst(vA, OpndSize_32, tmpValue);
        compileTable[entry].refCount--;
#ifdef DEBUG_CONST
        LOGD("getConstInfo: set VR %d to %d", vA, infoArray[entry].value[0]);
#endif
        return true;
    case OP_CONST:
        vA = INST_AA(inst);
        tmp_u4 = FETCH(1);
        tmp_u4 |= (u4)FETCH(2) << 16;
        entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
        infoArray[entry].isConst = true;
        infoArray[entry].value[0] = (s4)tmp_u4;
        tmpValue[0] = infoArray[entry].value[0];
        setVRToConst(vA, OpndSize_32, tmpValue);
        compileTable[entry].refCount--;
#ifdef DEBUG_CONST
        LOGD("getConstInfo: set VR %d to %d", vA, infoArray[entry].value[0]);
#endif
        return true;
    case OP_CONST_HIGH16:
        vA = INST_AA(inst);
        tmp_u2 = FETCH(1);
        entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
        infoArray[entry].isConst = true;
        infoArray[entry].value[0] = (s4)tmp_u2<<16;
        tmpValue[0] = infoArray[entry].value[0];
        setVRToConst(vA, OpndSize_32, tmpValue);
        compileTable[entry].refCount--;
#ifdef DEBUG_CONST
        LOGD("getConstInfo: set VR %d to %d", vA, infoArray[entry].value[0]);
#endif
        return true;
    case OP_CONST_WIDE_16:
        vA = INST_AA(inst);
        tmp_u2 = FETCH(1);
        entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
        infoArray[entry].isConst = true;
        infoArray[entry].value[0] = (s2)tmp_u2;
        tmpValue[0] = infoArray[entry].value[0];
        compileTable[entry].refCount--;
#ifdef DEBUG_CONST
        LOGD("getConstInfo: set VR %d to %x", vA, infoArray[entry].value[0]);
#endif

        entry = findVirtualRegInTable(vA+1, LowOpndRegType_gp, true);
        infoArray[entry].isConst = true;
        infoArray[entry].value[0] = (s2)tmp_u2>>31;
        tmpValue[1] = infoArray[entry].value[0];
        setVRToConst(vA, OpndSize_64, tmpValue);
        compileTable[entry].refCount--;
#ifdef DEBUG_CONST
        LOGD("getConstInfo: set VR %d to %x", vA+1, infoArray[entry].value[0]);
#endif
        return true;
    case OP_CONST_WIDE_32:
        vA = INST_AA(inst);
        tmp_u4 = FETCH(1);
        tmp_u4 |= (u4)FETCH(2) << 16;
        entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
        infoArray[entry].isConst = true;
        infoArray[entry].value[0] = (s4)tmp_u4;
        tmpValue[0] = infoArray[entry].value[0];
        compileTable[entry].refCount--;
#ifdef DEBUG_CONST
        LOGD("getConstInfo: set VR %d to %x", vA, infoArray[entry].value[0]);
#endif

        entry = findVirtualRegInTable(vA+1, LowOpndRegType_gp, true);
        infoArray[entry].isConst = true;
        infoArray[entry].value[0] = (s4)tmp_u4>>31;
        tmpValue[1] = infoArray[entry].value[0];
        setVRToConst(vA, OpndSize_64, tmpValue);
        compileTable[entry].refCount--;
#ifdef DEBUG_CONST
        LOGD("getConstInfo: set VR %d to %x", vA+1, infoArray[entry].value[0]);
#endif
        return true;
    case OP_CONST_WIDE:
        vA = INST_AA(inst);
        tmp_u4 = FETCH(1);
        tmp_u4 |= (u8)FETCH(2) << 16;
        entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
        infoArray[entry].isConst = true;
        infoArray[entry].value[0] = (s4)tmp_u4;
        tmpValue[0] = infoArray[entry].value[0];
        compileTable[entry].refCount--;
#ifdef DEBUG_CONST
        LOGD("getConstInfo: set VR %d to %x", vA, infoArray[entry].value[0]);
#endif

        tmp_u4 = (u8)FETCH(3);
        tmp_u4 |= (u8)FETCH(4) << 16;
        entry = findVirtualRegInTable(vA+1, LowOpndRegType_gp, true);
        infoArray[entry].isConst = true;
        infoArray[entry].value[0] = (s4)tmp_u4;
        tmpValue[1] = infoArray[entry].value[0];
        setVRToConst(vA, OpndSize_64, tmpValue);
        compileTable[entry].refCount--;
#ifdef DEBUG_CONST
        LOGD("getConstInfo: set VR %d to %x", vA+1, infoArray[entry].value[0]);
#endif
        return true;
    case OP_CONST_WIDE_HIGH16:
        vA = INST_AA(inst);
        tmp_u2 = FETCH(1);
        entry = findVirtualRegInTable(vA, LowOpndRegType_gp, true);
        infoArray[entry].isConst = true;
        infoArray[entry].value[0] = 0;
        tmpValue[0] = infoArray[entry].value[0];
        compileTable[entry].refCount--;
#ifdef DEBUG_CONST
        LOGD("getConstInfo: set VR %d to %x", vA, infoArray[entry].value[0]);
#endif

        entry = findVirtualRegInTable(vA+1, LowOpndRegType_gp, true);
        infoArray[entry].isConst = true;
        infoArray[entry].value[0] = (s4)tmp_u2<<16;
        tmpValue[1] = infoArray[entry].value[0];
        setVRToConst(vA, OpndSize_64, tmpValue);
        compileTable[entry].refCount--;
#ifdef DEBUG_CONST
        LOGD("getConstInfo: set VR %d to %x", vA+1, infoArray[entry].value[0]);
#endif
        return true;
#ifdef SUPPORT_HLO
    case OP_X_AGET_QUICK:
    case OP_X_AGET_OBJECT_QUICK:
    case OP_X_AGET_BOOLEAN_QUICK:
    case OP_X_AGET_BYTE_QUICK:
    case OP_X_AGET_CHAR_QUICK:
    case OP_X_AGET_SHORT_QUICK:
        vA = FETCH(1) & 0xff;
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        return false;
    case OP_X_AGET_WIDE_QUICK:
        vA = FETCH(1) & 0xff;
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        constWorklist[num_const_worklist] = vA+1;
        num_const_worklist++;
        return false;
    case OP_X_DEREF_GET:
    case OP_X_DEREF_GET_OBJECT:
    case OP_X_DEREF_GET_BOOLEAN:
    case OP_X_DEREF_GET_BYTE:
    case OP_X_DEREF_GET_CHAR:
    case OP_X_DEREF_GET_SHORT:
        vA = FETCH(1) & 0xff;
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        return false;
    case OP_X_DEREF_GET_WIDE:
        vA = FETCH(1) & 0xff;
        constWorklist[num_const_worklist] = vA;
        num_const_worklist++;
        constWorklist[num_const_worklist] = vA+1;
        num_const_worklist++;
        return false;
#endif
    }
    return false;
}

//! This function updates infoArray with virtual registers accessed when lowering the bytecode, and returns size of the bytecode in unit of u2

//! uses of virtual registers are added to infoArray first
int getVirtualRegInfo(VirtualRegInfo* infoArray) {
    u2 inst_op = INST_INST(inst);
    u2 vA = 0, vB = 0, vref, vindex;
    u2 v1, v2, length, vD, vG, vE, vF, count;
    u4 v1_u4, v2_u4;
    int kk, num, num_entry;
    s4 tmp_s4;
    s2 tmp_s2;
    u4 tmp_u4;
    int codeSize = 0;
    num_regs_per_bytecode = 0;
    //update infoArray[xx].allocConstraints
    for(num = 0; num < MAX_REG_PER_BYTECODE; num++) {
        for(kk = 0; kk < 8; kk++) {
            infoArray[num].allocConstraints[kk].physicalReg = (PhysicalReg)kk;
            infoArray[num].allocConstraints[kk].count = 0;
        }
    }

    switch (inst_op) {
    case OP_NOP:
        codeSize = 1;
        break;
    case OP_MOVE:
    case OP_MOVE_OBJECT:
    case OP_MOVE_FROM16:
    case OP_MOVE_OBJECT_FROM16:
    case OP_MOVE_16:
    case OP_MOVE_OBJECT_16:
        if(inst_op == OP_MOVE || inst_op == OP_MOVE_OBJECT) {
            vA = INST_A(inst);
            vB = INST_B(inst);
            codeSize = 1;
        }
        else if(inst_op == OP_MOVE_FROM16 || inst_op == OP_MOVE_OBJECT_FROM16) {
            vA = INST_AA(inst);
            vB = FETCH(1);
            codeSize = 2;
        }
        else if(inst_op == OP_MOVE_16 || inst_op == OP_MOVE_OBJECT_16) {
            vA = FETCH(1);
            vB = FETCH(2);
            codeSize = 3;
        }
        infoArray[1].regNum = vA; //dst
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = vB; //src
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        break;
    case OP_MOVE_WIDE:
    case OP_MOVE_WIDE_FROM16:
    case OP_MOVE_WIDE_16:
        if(inst_op == OP_MOVE_WIDE) {
            vA = INST_A(inst);
            vB = INST_B(inst);
            codeSize = 1;
        }
        else if(inst_op == OP_MOVE_WIDE_FROM16) {
            vA = INST_AA(inst);
            vB = FETCH(1);
            codeSize = 2;
        }
        else if(inst_op == OP_MOVE_WIDE_16) {
            vA = FETCH(1);
            vB = FETCH(2);
            codeSize = 3;
        }
        infoArray[1].regNum = vA; //dst
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_xmm;
        infoArray[0].regNum = vB; //src
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        num_regs_per_bytecode = 2;
        break;
    case OP_MOVE_RESULT: //access memory
    case OP_MOVE_RESULT_OBJECT:
        vA = INST_AA(inst);
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_gp;
        codeSize = 1;
        num_regs_per_bytecode = 1;
        break;
    case OP_MOVE_RESULT_WIDE: //note: 2 destinations
        vA = INST_AA(inst);
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        codeSize = 1;
        num_regs_per_bytecode = 1;
        break;
    case OP_MOVE_EXCEPTION: //access memory
        vA = INST_AA(inst);
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_gp;
        codeSize = 1;
        num_regs_per_bytecode = 1;
        break;
    case OP_RETURN_VOID:
    case OP_RETURN_VOID_BARRIER:
        codeSize = 1;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        num_regs_per_bytecode = 0;
        break;
    case OP_RETURN:
    case OP_RETURN_OBJECT:
        vA = INST_AA(inst);
        codeSize = 1;
        infoArray[0].regNum = vA; //src
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        num_regs_per_bytecode = 1;
        break;
    case OP_RETURN_WIDE:
        vA = INST_AA(inst);
        infoArray[0].regNum = vA; //src
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        num_regs_per_bytecode = 1;
        codeSize = 1;
        break;
    case OP_CONST_4:
        vA = INST_A(inst);
        tmp_s4 = (s4) (INST_B(inst) << 28) >> 28;
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 1;
        codeSize = 1;
        break;
    case OP_CONST_16:
        vA = INST_AA(inst);
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 1;
        codeSize = 2;
        break;
    case OP_CONST:
        vA = INST_AA(inst);
        tmp_u4 = FETCH(1);
        tmp_u4 |= (u4)FETCH(2) << 16;
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 1;
        codeSize = 3;
        break;
    case OP_CONST_HIGH16:
        vA = INST_AA(inst);
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 1;
        codeSize = 2;
        break;
    case OP_CONST_WIDE_16:
        vA = INST_AA(inst);
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = vA+1;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        codeSize = 2;
        num_regs_per_bytecode = 2;
        break;
    case OP_CONST_WIDE_32:
        vA = INST_AA(inst);
        tmp_u4 = FETCH(1);
        tmp_u4 |= (u4)FETCH(2) << 16;
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = vA+1;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        codeSize = 3;
        break;
    case OP_CONST_WIDE:
        vA = INST_AA(inst);
        tmp_u4 = FETCH(1);
        tmp_u4 |= (u8)FETCH(2) << 16;
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_gp;
        tmp_u4 = (u8)FETCH(3);
        tmp_u4 |= (u8)FETCH(4) << 16;
        infoArray[1].regNum = vA+1;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        codeSize = 5;
        num_regs_per_bytecode = 2;
        break;
    case OP_CONST_WIDE_HIGH16:
        vA = INST_AA(inst);
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = vA+1;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        codeSize = 2;
        break;
    case OP_CONST_STRING:
    case OP_CONST_STRING_JUMBO:
    case OP_CONST_CLASS:
        vA = INST_AA(inst);
        if(inst_op == OP_CONST_STRING || inst_op == OP_CONST_CLASS)
            codeSize = 2;
        else if(inst_op == OP_CONST_STRING_JUMBO)
            codeSize = 3;
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[0].allocConstraints[PhysicalReg_EAX].count = 1;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        num_regs_per_bytecode = 1;
        break;
    case OP_MONITOR_ENTER:
        vA = INST_AA(inst);
        codeSize = 1;
        infoArray[0].regNum = vA; //src
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 1;
        break;
    case OP_MONITOR_EXIT:
        vA = INST_AA(inst);
        codeSize = 1;
        infoArray[0].regNum = vA; //src
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        updateCurrentBBWithConstraints(PhysicalReg_EAX); //eax is used as return value from c function
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = 1;
        break;
    case OP_CHECK_CAST:
        codeSize = 2;
        vA = INST_AA(inst);
        infoArray[0].regNum = vA; //src
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_ECX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = 1;
        break;
    case OP_INSTANCE_OF:
        codeSize = 2;
        vA = INST_A(inst);
        vB = INST_B(inst);
        infoArray[0].regNum = vB; //src
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = vA; //dst
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        num_regs_per_bytecode = 2;
        break;
    case OP_ARRAY_LENGTH:
        vA = INST_A(inst);
        vB = INST_B(inst);
        codeSize = 1;
        infoArray[0].regNum = vB; //src
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = vA; //dst
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        //%edx is used in this bytecode, update currentBB->allocConstraints
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = 2;
        break;
    case OP_NEW_INSTANCE:
        vA = INST_AA(inst);
        infoArray[0].regNum = vA; //dst
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_D;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[0].allocConstraints[PhysicalReg_EAX].count = 1;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_ECX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = 1;
        codeSize = 2;
        break;
    case OP_NEW_ARRAY:
        vA = INST_A(inst); //destination
        vB = INST_B(inst); //length
        infoArray[0].regNum = vB; //src
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = vA; //dst
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[1].allocConstraints[PhysicalReg_EAX].count = 1;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = 2;
        codeSize = 2;
        break;
    case OP_FILLED_NEW_ARRAY: {//update return value
        //can use up to 5 registers to fill the content of array
        length = INST_B(inst);
        u2 vv = FETCH(2);
        v1 = vv & 0xf;
        v2 = (vv >> 4) & 0xf;
        u2 v3 = (vv >> 8) & 0xf;
        u2 v4 = (vv >> 12) & 0xf;
        u2 v5 = INST_A(inst);
        if(length >= 1) {
            infoArray[0].regNum = v1; //src
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_U;
            infoArray[0].physicalType = LowOpndRegType_gp;
        }
        if(length >= 2) {
            infoArray[1].regNum = v2; //src
            infoArray[1].refCount = 1;
            infoArray[1].accessType = REGACCESS_U;
            infoArray[1].physicalType = LowOpndRegType_gp;
        }
        if(length >= 3) {
            infoArray[2].regNum = v3; //src
            infoArray[2].refCount = 1;
            infoArray[2].accessType = REGACCESS_U;
            infoArray[2].physicalType = LowOpndRegType_gp;
        }
        if(length >= 4) {
            infoArray[3].regNum = v4; //src
            infoArray[3].refCount = 1;
            infoArray[3].accessType = REGACCESS_U;
            infoArray[3].physicalType = LowOpndRegType_gp;
        }
        if(length >= 5) {
            infoArray[4].regNum = v5; //src
            infoArray[4].refCount = 1;
            infoArray[4].accessType = REGACCESS_U;
            infoArray[4].physicalType = LowOpndRegType_gp;
        }
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = length;
        codeSize = 3;
        break;
    }
    case OP_FILLED_NEW_ARRAY_RANGE: {//use "length" virtual registers
        length = INST_AA(inst);
        u4 vC = (u4)FETCH(2);
        for(kk = 0; kk < length; kk++) {
            infoArray[kk].regNum = vC+kk; //src
            infoArray[kk].refCount = 1;
            infoArray[kk].accessType = REGACCESS_U;
            infoArray[kk].physicalType = LowOpndRegType_gp;
        }
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = length;
        codeSize = 3;
        break;
    }
    case OP_FILL_ARRAY_DATA: //update content of array, read memory
        vA = INST_AA(inst); //use virtual register, but has side-effect, update memory
        infoArray[0].regNum = vA; //use
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = 1;
        codeSize = 3;
        break;
    case OP_THROW: //update glue->exception
        vA = INST_AA(inst);
        infoArray[0].regNum = vA; //use
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = 1;
        codeSize = 1;
        break;
    case OP_THROW_VERIFICATION_ERROR:
        num_regs_per_bytecode = 0;
        codeSize = 2;
        break;
    case OP_GOTO:
        codeSize = 1;
        num_regs_per_bytecode = 0;
        break;
    case OP_GOTO_16:
        codeSize = 2;
        num_regs_per_bytecode = 0;
        break;
    case OP_GOTO_32:
        codeSize = 3;
        num_regs_per_bytecode = 0;
        break;
    case OP_PACKED_SWITCH:
    case OP_SPARSE_SWITCH:
        vA = INST_AA(inst);
        codeSize = 3;
        infoArray[0].regNum = vA; //use
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = 1;
        break;

    case OP_CMPL_FLOAT: //move 32 bits from memory to lower part of XMM register
    case OP_CMPG_FLOAT:
        codeSize = 2;
        vA = INST_AA(inst);
        v1_u4 = FETCH(1) & 0xff;
        v2_u4 = FETCH(1) >> 8;
        num_regs_per_bytecode = 1;
        infoArray[0].regNum = v1_u4; //use ss or sd CHECK
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_ss;
        infoArray[1].regNum = v2_u4; //use
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_ss;
        num_regs_per_bytecode = 3;
        num_entry = 2;
        infoArray[num_entry].regNum = vA; //define
        infoArray[num_entry].refCount = 1;
        infoArray[num_entry].accessType = REGACCESS_D;
        infoArray[num_entry].physicalType = LowOpndRegType_gp;
        break;
    case OP_CMPL_DOUBLE: //move 64 bits from memory to lower part of XMM register
    case OP_CMPG_DOUBLE:
    case OP_CMP_LONG: //load v1, v1+1, v2, v2+1 to gpr
        codeSize = 2;
        vA = INST_AA(inst);
        v1_u4 = FETCH(1) & 0xff;
        v2_u4 = FETCH(1) >> 8;
        num_regs_per_bytecode = 1;
        if(inst_op == OP_CMP_LONG) {
            infoArray[0].regNum = v1_u4; //use
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_U;
            infoArray[0].physicalType = LowOpndRegType_gp;
            infoArray[1].regNum = v1_u4 + 1; //use
            infoArray[1].refCount = 1;
            infoArray[1].accessType = REGACCESS_U;
            infoArray[1].physicalType = LowOpndRegType_gp;
            infoArray[2].regNum = v2_u4; //use
            infoArray[2].refCount = 1;
            infoArray[2].accessType = REGACCESS_U;
            infoArray[2].physicalType = LowOpndRegType_gp;
            infoArray[3].regNum = v2_u4 + 1; //use
            infoArray[3].refCount = 1;
            infoArray[3].accessType = REGACCESS_U;
            infoArray[3].physicalType = LowOpndRegType_gp;
            num_regs_per_bytecode = 5;
            num_entry = 4;
            infoArray[num_entry].regNum = vA; //define
            infoArray[num_entry].refCount = 2;
            infoArray[num_entry].accessType = REGACCESS_D;
            infoArray[num_entry].physicalType = LowOpndRegType_gp;
        }
        else {
            infoArray[0].regNum = v1_u4; //use ss or sd CHECK
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_U;
            infoArray[0].physicalType = LowOpndRegType_xmm;
            infoArray[1].regNum = v2_u4; //use
            infoArray[1].refCount = 1;
            infoArray[1].accessType = REGACCESS_U;
            infoArray[1].physicalType = LowOpndRegType_xmm;
            num_regs_per_bytecode = 3;
            num_entry = 2;
            infoArray[num_entry].regNum = vA; //define
            infoArray[num_entry].refCount = 1;
            infoArray[num_entry].accessType = REGACCESS_D;
            infoArray[num_entry].physicalType = LowOpndRegType_gp;
        }
        break;
    case OP_IF_EQ:
    case OP_IF_NE:
    case OP_IF_LT:
    case OP_IF_GE:
    case OP_IF_GT:
    case OP_IF_LE:
        vA = INST_A(inst);
        vB = INST_B(inst);
        infoArray[0].regNum = vA; //use
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = vB;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        codeSize =12;
        break;
    case OP_IF_EQZ:
    case OP_IF_NEZ:
    case OP_IF_LTZ:
    case OP_IF_GEZ:
    case OP_IF_GTZ:
    case OP_IF_LEZ:
        vA = INST_AA(inst);
        infoArray[0].regNum = vA; //use
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 1;
        codeSize = 2;
        break;
    case OP_AGET:
        codeSize = 2;
    case OP_AGET_WIDE:
        codeSize = 2;
    case OP_AGET_OBJECT:
        codeSize = 2;
    case OP_AGET_BOOLEAN: //movez 8
        codeSize = 2;
    case OP_AGET_BYTE: //moves 8
        codeSize = 2;
    case OP_AGET_CHAR: //movez 16
        codeSize = 2;
    case OP_AGET_SHORT: //moves 16
        codeSize = 2;
        vA = INST_AA(inst);
        vref = FETCH(1) & 0xff;
        vindex = FETCH(1) >> 8;
        if(inst_op == OP_AGET_WIDE) {
            infoArray[2].regNum = vA;
            infoArray[2].refCount = 1;
            infoArray[2].accessType = REGACCESS_D;
            infoArray[2].physicalType = LowOpndRegType_xmm; //64, 128 not used in lowering
        } else {
            infoArray[2].regNum = vA;
            infoArray[2].refCount = 1;
            infoArray[2].accessType = REGACCESS_D;
            infoArray[2].physicalType = LowOpndRegType_gp;
        }
        infoArray[0].regNum = vref; //use
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = vindex; //use
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 3;
        break;
    case OP_APUT:
    case OP_APUT_WIDE:
    case OP_APUT_OBJECT:
    case OP_APUT_BOOLEAN:
    case OP_APUT_BYTE:
    case OP_APUT_CHAR:
    case OP_APUT_SHORT:
        vA = INST_AA(inst);
        vref = FETCH(1) & 0xff;
        vindex = FETCH(1) >> 8;
        codeSize = 2;
        if(inst_op == OP_APUT_WIDE) {
            infoArray[0].regNum = vA;
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_U;
            infoArray[0].physicalType = LowOpndRegType_xmm; //64, 128 not used in lowering
        } else {
            infoArray[0].regNum = vA;
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_U;
            infoArray[0].physicalType = LowOpndRegType_gp;
        }
        infoArray[1].regNum = vref; //use
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = vindex; //use
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_U;
        infoArray[2].physicalType = LowOpndRegType_gp;
        if(inst_op == OP_APUT_OBJECT) {
            updateCurrentBBWithConstraints(PhysicalReg_EAX);
            updateCurrentBBWithConstraints(PhysicalReg_EDX);
        }
        num_regs_per_bytecode = 3;
        break;

    case OP_IGET:
    case OP_IGET_WIDE:
    case OP_IGET_OBJECT:
    case OP_IGET_VOLATILE:
    case OP_IGET_WIDE_VOLATILE:
    case OP_IGET_OBJECT_VOLATILE:
    case OP_IGET_BOOLEAN:
    case OP_IGET_BYTE:
    case OP_IGET_CHAR:
    case OP_IGET_SHORT:
    case OP_IGET_QUICK:
    case OP_IGET_WIDE_QUICK:
    case OP_IGET_OBJECT_QUICK:
        vA = INST_A(inst);
        vB = INST_B(inst);
        codeSize = 2;
        if(inst_op == OP_IGET_WIDE || inst_op == OP_IGET_WIDE_QUICK) {
            infoArray[1].regNum = vA;
            infoArray[1].refCount = 1;
            infoArray[1].accessType = REGACCESS_D;
            infoArray[1].physicalType = LowOpndRegType_xmm; //64
        } else if(inst_op == OP_IGET_WIDE_VOLATILE) {
            infoArray[1].regNum = vA;
            infoArray[1].refCount = 1;
            infoArray[1].accessType = REGACCESS_D;
            infoArray[1].physicalType = LowOpndRegType_gp;
            infoArray[2].regNum = vA+1;
            infoArray[2].refCount = 1;
            infoArray[2].accessType = REGACCESS_D;
            infoArray[2].physicalType = LowOpndRegType_gp;
        } else {
            infoArray[1].regNum = vA;
            infoArray[1].refCount = 1;
            infoArray[1].accessType = REGACCESS_D;
            infoArray[1].physicalType = LowOpndRegType_gp;
        }
        infoArray[0].regNum = vB; //object instance
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        if(inst_op == OP_IGET_WIDE_VOLATILE)
            num_regs_per_bytecode = 3;
        else
            num_regs_per_bytecode = 2;
        break;
    case OP_IPUT:
    case OP_IPUT_WIDE:
    case OP_IPUT_OBJECT:
    case OP_IPUT_VOLATILE:
    case OP_IPUT_WIDE_VOLATILE:
    case OP_IPUT_OBJECT_VOLATILE:
    case OP_IPUT_BOOLEAN:
    case OP_IPUT_BYTE:
    case OP_IPUT_CHAR:
    case OP_IPUT_SHORT:
    case OP_IPUT_QUICK:
    case OP_IPUT_WIDE_QUICK:
    case OP_IPUT_OBJECT_QUICK:
        vA = INST_A(inst);
        vB = INST_B(inst);
        codeSize = 2;
        if(inst_op == OP_IPUT_WIDE || inst_op == OP_IPUT_WIDE_QUICK || inst_op == OP_IPUT_WIDE_VOLATILE) {
            infoArray[0].regNum = vA;
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_U;
            infoArray[0].physicalType = LowOpndRegType_xmm; //64
        } else {
            infoArray[0].regNum = vA;
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_U;
            infoArray[0].physicalType = LowOpndRegType_gp;
        }
        infoArray[1].regNum = vB; //object instance
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = 2;
        break;
    case OP_SGET:
    case OP_SGET_WIDE:
    case OP_SGET_OBJECT:
    case OP_SGET_VOLATILE:
    case OP_SGET_WIDE_VOLATILE:
    case OP_SGET_OBJECT_VOLATILE:
    case OP_SGET_BOOLEAN:
    case OP_SGET_BYTE:
    case OP_SGET_CHAR:
    case OP_SGET_SHORT:
        vA = INST_AA(inst);
        codeSize = 2;
        if(inst_op == OP_SGET_WIDE) {
            infoArray[0].regNum = vA;
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_D;
            infoArray[0].physicalType = LowOpndRegType_xmm; //64
        } else if(inst_op == OP_SGET_WIDE_VOLATILE) {
            infoArray[0].regNum = vA;
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_D;
            infoArray[0].physicalType = LowOpndRegType_gp;
            infoArray[1].regNum = vA+1;
            infoArray[1].refCount = 1;
            infoArray[1].accessType = REGACCESS_D;
            infoArray[1].physicalType = LowOpndRegType_gp;
        } else {
            infoArray[0].regNum = vA;
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_D;
            infoArray[0].physicalType = LowOpndRegType_gp;
        }
        if(inst_op == OP_SGET_WIDE_VOLATILE)
            num_regs_per_bytecode = 2;
        else
            num_regs_per_bytecode = 1;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        break;
    case OP_SPUT:
    case OP_SPUT_WIDE:
    case OP_SPUT_OBJECT:
    case OP_SPUT_VOLATILE:
    case OP_SPUT_WIDE_VOLATILE:
    case OP_SPUT_OBJECT_VOLATILE:
    case OP_SPUT_BOOLEAN:
    case OP_SPUT_BYTE:
    case OP_SPUT_CHAR:
    case OP_SPUT_SHORT:
        vA = INST_AA(inst);
        codeSize = 2;
        if(inst_op == OP_SPUT_WIDE || inst_op == OP_SPUT_WIDE_VOLATILE) {
            infoArray[0].regNum = vA;
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_U;
            infoArray[0].physicalType = LowOpndRegType_xmm; //64
        } else {
            infoArray[0].regNum = vA;
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_U;
            infoArray[0].physicalType = LowOpndRegType_gp;
        }
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        num_regs_per_bytecode = 1;
        break;

    case OP_INVOKE_VIRTUAL:
    case OP_INVOKE_SUPER:
    case OP_INVOKE_DIRECT:
    case OP_INVOKE_STATIC:
    case OP_INVOKE_INTERFACE:
    case OP_INVOKE_VIRTUAL_QUICK:
    case OP_INVOKE_SUPER_QUICK:
        codeSize = 3;
        vD = FETCH(2) & 0xf; //object for virtual,direct & interface
        count = INST_B(inst);
        vE = (FETCH(2) >> 4) & 0xf;
        vF = (FETCH(2) >> 8) & 0xf;
        vG = (FETCH(2) >> 12) & 0xf;
        vA = INST_A(inst); //5th argument
        if(count == 0) {
            if(inst_op == OP_INVOKE_VIRTUAL || inst_op == OP_INVOKE_DIRECT ||
               inst_op == OP_INVOKE_INTERFACE || inst_op == OP_INVOKE_VIRTUAL_QUICK ||
               inst_op == OP_INVOKE_SUPER_QUICK) {
                infoArray[0].regNum = vD;
                infoArray[0].refCount = 1;
                infoArray[0].accessType = REGACCESS_U;
                infoArray[0].physicalType = LowOpndRegType_gp;
                num_regs_per_bytecode = 1;
            }
            num_regs_per_bytecode = 0;
        }
        else num_regs_per_bytecode = count;
        if(count >= 1) {
            infoArray[0].regNum = vD;
            if(inst_op == OP_INVOKE_VIRTUAL_QUICK ||
               inst_op == OP_INVOKE_SUPER_QUICK) {
                infoArray[0].refCount = 2;
            } else if(inst_op == OP_INVOKE_VIRTUAL || inst_op == OP_INVOKE_DIRECT || inst_op == OP_INVOKE_INTERFACE) {
                infoArray[0].refCount = 2;
            } else {
                infoArray[0].refCount = 1;
            }
            infoArray[0].accessType = REGACCESS_U;
            infoArray[0].physicalType = LowOpndRegType_gp;
        }
        if(count >= 2) {
            infoArray[1].regNum = vE;
            infoArray[1].refCount = 1;
            infoArray[1].accessType = REGACCESS_U;
            infoArray[1].physicalType = LowOpndRegType_gp;
        }
        if(count >= 3) {
            infoArray[2].regNum = vF;
            infoArray[2].refCount = 1;
            infoArray[2].accessType = REGACCESS_U;
            infoArray[2].physicalType = LowOpndRegType_gp;
        }
        if(count >= 4) {
            infoArray[3].regNum = vG;
            infoArray[3].refCount = 1;
            infoArray[3].accessType = REGACCESS_U;
            infoArray[3].physicalType = LowOpndRegType_gp;
        }
        if(count >= 5) {
            infoArray[4].regNum = vA;
            infoArray[4].refCount = 1;
            infoArray[4].accessType = REGACCESS_U;
            infoArray[4].physicalType = LowOpndRegType_gp;
        }
        if(inst_op != OP_INVOKE_VIRTUAL_QUICK && inst_op != OP_INVOKE_SUPER_QUICK)
            updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_ECX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        break;
    case OP_INVOKE_VIRTUAL_RANGE:
    case OP_INVOKE_SUPER_RANGE:
    case OP_INVOKE_DIRECT_RANGE:
    case OP_INVOKE_STATIC_RANGE:
    case OP_INVOKE_INTERFACE_RANGE:
    case OP_INVOKE_VIRTUAL_QUICK_RANGE:
    case OP_INVOKE_SUPER_QUICK_RANGE:
        codeSize = 3;
        vD = FETCH(2);
        count = INST_AA(inst);
        if(count == 0) {
            if(inst_op == OP_INVOKE_VIRTUAL_RANGE || inst_op == OP_INVOKE_DIRECT_RANGE ||
               inst_op == OP_INVOKE_INTERFACE_RANGE || inst_op == OP_INVOKE_VIRTUAL_QUICK_RANGE ||
               inst_op == OP_INVOKE_SUPER_QUICK_RANGE) {
                infoArray[0].regNum = vD;
                infoArray[0].refCount = 1;
                infoArray[0].accessType = REGACCESS_U;
                infoArray[0].physicalType = LowOpndRegType_gp;
            }
        }
        if(count > 0) { //same for count > 10
            for(kk = 0; kk < count; kk++) {
                infoArray[kk].regNum = vD+kk; //src
                if(kk == 0 && (inst_op == OP_INVOKE_VIRTUAL_QUICK_RANGE ||
                               inst_op == OP_INVOKE_SUPER_QUICK_RANGE))
                    infoArray[kk].refCount = 2;
                else if(kk == 0 && (inst_op == OP_INVOKE_VIRTUAL_RANGE ||
                                    inst_op == OP_INVOKE_DIRECT_RANGE ||
                                    inst_op == OP_INVOKE_INTERFACE_RANGE))
                    infoArray[kk].refCount = 2;
                else
                    infoArray[kk].refCount = 1;
                infoArray[kk].accessType = REGACCESS_U;
                infoArray[kk].physicalType = LowOpndRegType_gp;
            }
        }
        if(inst_op != OP_INVOKE_VIRTUAL_QUICK_RANGE && inst_op != OP_INVOKE_SUPER_QUICK_RANGE)
            updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_ECX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = count;
        break;
    case OP_NEG_INT:
    case OP_NOT_INT:
    case OP_NEG_FLOAT:
        vA = INST_A(inst); //destination
        vB = INST_B(inst);
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = vB;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        codeSize = 1;
        break;
    case OP_NEG_LONG:
    case OP_NOT_LONG:
    case OP_NEG_DOUBLE:
        vA = INST_A(inst); //destination
        vB = INST_B(inst);
        codeSize = 1;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_xmm;
        infoArray[0].regNum = vB;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        num_regs_per_bytecode = 2;
        break;
    case OP_INT_TO_LONG: //hard-coded registers
        vA = INST_A(inst); //destination
        vB = INST_B(inst);
        codeSize = 1;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp; //save from %eax
        infoArray[1].allocConstraints[PhysicalReg_EAX].count = 1;
        infoArray[2].regNum = vA+1;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[2].allocConstraints[PhysicalReg_EDX].count = 1;
        infoArray[0].regNum = vB;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[0].allocConstraints[PhysicalReg_EAX].count = 1;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = 3;
        break;
    case OP_INT_TO_FLOAT: //32 to 32
    case OP_INT_TO_DOUBLE: //32 to 64
    case OP_LONG_TO_FLOAT: //64 to 32
    case OP_LONG_TO_DOUBLE: //64 to 64
    case OP_FLOAT_TO_DOUBLE: //32 to 64
    case OP_DOUBLE_TO_FLOAT: //64 to 32
        vA = INST_A(inst); //destination
        vB = INST_B(inst);
        codeSize = 1;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        if(inst_op == OP_INT_TO_DOUBLE || inst_op == OP_LONG_TO_DOUBLE || inst_op == OP_FLOAT_TO_DOUBLE)
            infoArray[1].physicalType = LowOpndRegType_fs;
        else
            infoArray[1].physicalType = LowOpndRegType_fs_s;
        infoArray[0].regNum = vB;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        if(inst_op == OP_INT_TO_FLOAT || inst_op == OP_INT_TO_DOUBLE || inst_op == OP_FLOAT_TO_DOUBLE)
            infoArray[0].physicalType = LowOpndRegType_fs_s; //float
        else
            infoArray[0].physicalType = LowOpndRegType_fs;
        num_regs_per_bytecode = 2;
        break;
    case OP_LONG_TO_INT:
        vA = INST_A(inst); //destination
        vB = INST_B(inst);
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = vB;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        codeSize = 1;
        break;
    case OP_FLOAT_TO_INT:
    case OP_DOUBLE_TO_INT: //for reaching-def analysis
        vA = INST_A(inst); //destination
        vB = INST_B(inst);
        codeSize = 1;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 3;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_fs_s; //store_int_fp_stack_VR
        infoArray[0].regNum = vB;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        if(inst_op == OP_DOUBLE_TO_INT)
            infoArray[0].physicalType = LowOpndRegType_fs;
        else
            infoArray[0].physicalType = LowOpndRegType_fs_s;
        num_regs_per_bytecode = 3;
        break;
    case OP_FLOAT_TO_LONG:
    case OP_DOUBLE_TO_LONG:
        vA = INST_A(inst); //destination
        vB = INST_B(inst);
        codeSize = 1;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 3;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_fs;
        infoArray[0].regNum = vB;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        if(inst_op == OP_DOUBLE_TO_LONG)
            infoArray[0].physicalType = LowOpndRegType_fs;
        else
            infoArray[0].physicalType = LowOpndRegType_fs_s;
        num_regs_per_bytecode = 3;
        break;
    case OP_INT_TO_BYTE:
    case OP_INT_TO_CHAR:
    case OP_INT_TO_SHORT:
        vA = INST_A(inst); //destination
        vB = INST_B(inst);
        codeSize = 1;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = vB;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        break;

    case OP_ADD_INT:
    case OP_SUB_INT:
    case OP_MUL_INT:
    case OP_AND_INT:
    case OP_OR_INT:
    case OP_XOR_INT:
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        codeSize = 2;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = v2;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 3;
        break;
    case OP_DIV_INT:
    case OP_REM_INT:
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        codeSize = 2;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 2;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = v2;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[0].allocConstraints[PhysicalReg_EAX].count = 1; //for v1
        if(inst_op == OP_REM_INT)
            infoArray[2].allocConstraints[PhysicalReg_EDX].count = 1;//vA
        else
            infoArray[2].allocConstraints[PhysicalReg_EAX].count = 1;//vA
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = 3;
        break;
    case OP_SHL_INT:
    case OP_SHR_INT:
    case OP_USHR_INT:
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        codeSize = 2;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = v2; // in ecx
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[1].allocConstraints[PhysicalReg_ECX].count = 1;
        updateCurrentBBWithConstraints(PhysicalReg_ECX);
        num_regs_per_bytecode = 3;
        break;
    case OP_ADD_LONG:
    case OP_SUB_LONG:
    case OP_AND_LONG:
    case OP_OR_LONG:
    case OP_XOR_LONG:
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        codeSize = 2;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_xmm;
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = v2;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_xmm;
        num_regs_per_bytecode = 3;
        break;
    case OP_MUL_LONG: //used int
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = v1+1;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = v2;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_U;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = v2+1;
        infoArray[3].refCount = 1;
        infoArray[3].accessType = REGACCESS_U;
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = vA;
        infoArray[4].refCount = 1;
        infoArray[4].accessType = REGACCESS_D;
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[5].regNum = vA+1;
        infoArray[5].refCount = 1;
        infoArray[5].accessType = REGACCESS_D;
        infoArray[5].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 6;
        codeSize = 2;
        break;
    case OP_DIV_LONG: //v1: xmm v2,vA:
    case OP_REM_LONG:
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = v2;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = v2+1;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_U;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = vA;
        infoArray[3].refCount = 1;
        infoArray[3].accessType = REGACCESS_D;
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = vA+1;
        infoArray[4].refCount = 1;
        infoArray[4].accessType = REGACCESS_D;
        infoArray[4].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 5;
        codeSize = 2;
        break;
    case OP_SHL_LONG: //v2: 32, move_ss; v1,vA: xmm CHECK
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = v2;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_ss;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_xmm;
        num_regs_per_bytecode = 3;
        codeSize = 2;
        break;
    case OP_SHR_LONG: //v2: 32, move_ss; v1,vA: xmm CHECK
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = v2;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_ss;
        infoArray[2].regNum = v1+1;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_U;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = vA;
        infoArray[3].refCount = 1;
        infoArray[3].accessType = REGACCESS_D;
        infoArray[3].physicalType = LowOpndRegType_xmm;
        num_regs_per_bytecode = 4;
        codeSize = 2;
        break;
    case OP_USHR_LONG: //v2: move_ss; v1,vA: move_sd
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm; //sd
        infoArray[1].regNum = v2;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_ss; //ss
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_xmm; //sd
        num_regs_per_bytecode = 3;
        codeSize = 2;
        break;
    case OP_ADD_FLOAT: //move_ss
    case OP_SUB_FLOAT:
    case OP_MUL_FLOAT:
    case OP_DIV_FLOAT:
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        codeSize = 2;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_ss;
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_ss;
        infoArray[1].regNum = v2;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_ss;
        num_regs_per_bytecode = 3;
        break;
    case OP_REM_FLOAT: //32 bit GPR, fp_stack for output
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        codeSize = 2;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_fs_s;
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = v2;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 3;
        break;
    case OP_ADD_DOUBLE: //move_sd
    case OP_SUB_DOUBLE:
    case OP_MUL_DOUBLE:
    case OP_DIV_DOUBLE:
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        codeSize = 2;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_xmm;
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = v2;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_xmm;
        num_regs_per_bytecode = 3;
        break;
    case OP_REM_DOUBLE: //64 bit XMM, fp_stack for output
        vA = INST_AA(inst);
        v1 = *((u1*)rPC + 2);
        v2 = *((u1*)rPC + 3);
        codeSize = 2;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_D;
        infoArray[2].physicalType = LowOpndRegType_fs;
        infoArray[0].regNum = v1;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = v2;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_xmm;
        num_regs_per_bytecode = 3;
        break;

    case OP_ADD_INT_2ADDR:
    case OP_SUB_INT_2ADDR:
    case OP_MUL_INT_2ADDR:
    case OP_AND_INT_2ADDR:
    case OP_OR_INT_2ADDR:
    case OP_XOR_INT_2ADDR:
        vA = INST_A(inst);
        v2 = INST_B(inst);
        codeSize = 1;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 2;
        infoArray[1].accessType = REGACCESS_UD; //use then define
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = v2;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        break;
    case OP_DIV_INT_2ADDR:
    case OP_REM_INT_2ADDR:
        vA = INST_A(inst);
        v2 = INST_B(inst);
        codeSize = 1;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 3;
        infoArray[1].accessType = REGACCESS_UD; //use then define
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = v2;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].allocConstraints[PhysicalReg_EAX].count = 1; //for v1 is vA
        if(inst_op == OP_REM_INT_2ADDR)
            infoArray[1].allocConstraints[PhysicalReg_EDX].count = 1;//vA
        else
            infoArray[1].allocConstraints[PhysicalReg_EAX].count = 1;//vA
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = 2;
        break;
    case OP_SHL_INT_2ADDR:
    case OP_SHR_INT_2ADDR:
    case OP_USHR_INT_2ADDR:
        vA = INST_A(inst);
        v2 = INST_B(inst);
        codeSize = 1;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 2;
        infoArray[1].accessType = REGACCESS_UD; //use then define
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = v2;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[0].allocConstraints[PhysicalReg_ECX].count = 1; //v2
        updateCurrentBBWithConstraints(PhysicalReg_ECX);
        num_regs_per_bytecode = 2;
        break;
    case OP_ADD_LONG_2ADDR:
    case OP_SUB_LONG_2ADDR:
    case OP_AND_LONG_2ADDR:
    case OP_OR_LONG_2ADDR:
    case OP_XOR_LONG_2ADDR:
        vA = INST_A(inst);
        v2 = INST_B(inst);
        codeSize = 1;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 2;
        infoArray[1].accessType = REGACCESS_UD;
        infoArray[1].physicalType = LowOpndRegType_xmm;
        infoArray[0].regNum = v2;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        num_regs_per_bytecode = 2;
        break;
    case OP_MUL_LONG_2ADDR:
        vA = INST_A(inst);
        v2 = INST_B(inst);
        codeSize = 1;
        num_regs_per_bytecode = 4;
        infoArray[0].regNum = v2;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = v2+1;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 2;
        infoArray[2].accessType = REGACCESS_UD;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = vA+1;
        infoArray[3].refCount = 2;
        infoArray[3].accessType = REGACCESS_UD;
        infoArray[3].physicalType = LowOpndRegType_gp;
        break;
    case OP_DIV_LONG_2ADDR: //vA used as xmm, then updated as gps
    case OP_REM_LONG_2ADDR:
        vA = INST_A(inst);
        v2 = INST_B(inst);
        num_regs_per_bytecode = 5;
        codeSize = 1;
        infoArray[0].regNum = vA;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = v2;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = v2+1;
        infoArray[2].refCount = 1;
        infoArray[2].accessType = REGACCESS_U;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = vA;
        infoArray[3].refCount = 1;
        infoArray[3].accessType = REGACCESS_D;
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = vA+1;
        infoArray[4].refCount = 1;
        infoArray[4].accessType = REGACCESS_D;
        infoArray[4].physicalType = LowOpndRegType_gp;
        break;
    case OP_SHL_LONG_2ADDR:
        vA = INST_A(inst);
        v2 = INST_B(inst);
        num_regs_per_bytecode = 2;
        codeSize = 1;
        infoArray[0].regNum = v2; //ss
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_ss;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 2;
        infoArray[1].accessType = REGACCESS_UD;
        infoArray[1].physicalType = LowOpndRegType_xmm;
        break;
    case OP_SHR_LONG_2ADDR:
        vA = INST_A(inst);
        v2 = INST_B(inst);
        num_regs_per_bytecode = 3;
        codeSize = 1;
        infoArray[0].regNum = v2; //ss
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_ss;
        infoArray[1].regNum = vA+1;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_U;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = vA;
        infoArray[2].refCount = 2;
        infoArray[2].accessType = REGACCESS_UD;
        infoArray[2].physicalType = LowOpndRegType_xmm;
        break;
    case OP_USHR_LONG_2ADDR:
        vA = INST_A(inst);
        v2 = INST_B(inst);
        num_regs_per_bytecode = 2;
        codeSize = 1;
        infoArray[0].regNum = v2;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_ss; //ss CHECK
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 2;
        infoArray[1].accessType = REGACCESS_UD;
        infoArray[1].physicalType = LowOpndRegType_xmm; //sd
        break;
    case OP_ADD_FLOAT_2ADDR:
    case OP_SUB_FLOAT_2ADDR:
    case OP_MUL_FLOAT_2ADDR:
    case OP_DIV_FLOAT_2ADDR:
        vA = INST_A(inst);
        v2 = INST_B(inst);
        codeSize = 1;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 2;
        infoArray[1].accessType = REGACCESS_UD;
        infoArray[1].physicalType = LowOpndRegType_ss;
        infoArray[0].regNum = v2;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_ss;
        num_regs_per_bytecode = 2;
        break;
    case OP_REM_FLOAT_2ADDR: //load vA as GPR, store from fs
        vA = INST_A(inst);
        v2 = INST_B(inst);
        codeSize = 1;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 2;
        infoArray[1].accessType = REGACCESS_UD;
        infoArray[1].physicalType = LowOpndRegType_gp; //CHECK
        infoArray[0].regNum = v2;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        break;
    case OP_ADD_DOUBLE_2ADDR:
    case OP_SUB_DOUBLE_2ADDR:
    case OP_MUL_DOUBLE_2ADDR:
    case OP_DIV_DOUBLE_2ADDR:
        vA = INST_A(inst);
        v2 = INST_B(inst);
        codeSize = 1;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 2;
        infoArray[1].accessType = REGACCESS_UD;
        infoArray[1].physicalType = LowOpndRegType_xmm;
        infoArray[0].regNum = v2;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        num_regs_per_bytecode = 2;
        break;
    case OP_REM_DOUBLE_2ADDR: //load to xmm, store from fs
        vA = INST_A(inst);
        v2 = INST_B(inst);
        codeSize = 1;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 2;
        infoArray[1].accessType = REGACCESS_UD;
        infoArray[1].physicalType = LowOpndRegType_xmm; //CHECK
        infoArray[0].regNum = v2;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        num_regs_per_bytecode = 2;
        break;

    case OP_ADD_INT_LIT16:
    case OP_RSUB_INT:
    case OP_MUL_INT_LIT16:
    case OP_AND_INT_LIT16:
    case OP_OR_INT_LIT16:
    case OP_XOR_INT_LIT16:
        vA = INST_A(inst);
        vB = INST_B(inst);
        codeSize = 2;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = vB;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        break;
    case OP_DIV_INT_LIT16:
    case OP_REM_INT_LIT16:
        vA = INST_A(inst);
        vB = INST_B(inst);
        codeSize = 2;
        tmp_s4 = (s2)FETCH(1);
        tmp_s2 = tmp_s4;
        if(tmp_s2 == 0) {
            num_regs_per_bytecode = 0;
            break;
        }
        infoArray[1].regNum = vA; //in edx for rem, in eax
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = vB; //in eax
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        if(inst_op == OP_DIV_INT_LIT16) {
            int power = isPowerOfTwo(tmp_s2);
            if(power >= 1) { /* divide by a power of 2 constant */
                infoArray[1].refCount = 1;
                break;
            }
        }
        if(tmp_s2 == -1)
            infoArray[1].refCount = 2;
        else
            infoArray[1].refCount = 1;
        if(inst_op == OP_REM_INT_LIT16)
            infoArray[1].allocConstraints[PhysicalReg_EDX].count = 1;
        else
            infoArray[1].allocConstraints[PhysicalReg_EAX].count = 1;
        infoArray[0].allocConstraints[PhysicalReg_EAX].count = 1;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        break;
    case OP_ADD_INT_LIT8:
    case OP_RSUB_INT_LIT8:
    case OP_MUL_INT_LIT8:
    case OP_AND_INT_LIT8:
    case OP_OR_INT_LIT8:
    case OP_XOR_INT_LIT8:
    case OP_SHL_INT_LIT8:
    case OP_SHR_INT_LIT8:
    case OP_USHR_INT_LIT8:
        codeSize = 2;
        vA = INST_AA(inst);
        vB = (u2)FETCH(1) & 0xff;
        infoArray[1].regNum = vA;
        infoArray[1].refCount = 1;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = vB;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        break;
    case OP_DIV_INT_LIT8:
    case OP_REM_INT_LIT8:
        codeSize = 2;
        vA = INST_AA(inst);
        vB = (u2)FETCH(1) & 0xff;
        tmp_s2 = (s2)FETCH(1) >> 8;
        if(tmp_s2 == 0) {
            num_regs_per_bytecode = 0;
            break;
        }

        infoArray[1].regNum = vA;
        infoArray[1].accessType = REGACCESS_D;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[0].regNum = vB;
        infoArray[0].refCount = 1;
        infoArray[0].accessType = REGACCESS_U;
        infoArray[0].physicalType = LowOpndRegType_gp;
        num_regs_per_bytecode = 2;
        if(inst_op == OP_DIV_INT_LIT8) {
            int power = isPowerOfTwo(tmp_s2);
            if(power >= 1) { /* divide by a power of 2 constant */
                infoArray[1].refCount = 1;
                break;
            }
        }

        if(tmp_s2 == -1)
            infoArray[1].refCount = 2;
        else
            infoArray[1].refCount = 1;
        if(inst_op == OP_REM_INT_LIT8)
            infoArray[1].allocConstraints[PhysicalReg_EDX].count = 1;
        else
            infoArray[1].allocConstraints[PhysicalReg_EAX].count = 1;
        infoArray[0].allocConstraints[PhysicalReg_EAX].count = 1;
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        break;
    case OP_EXECUTE_INLINE: //update glue->retval
    case OP_EXECUTE_INLINE_RANGE:
        u4 vC;
        if(inst_op == OP_EXECUTE_INLINE)
            num = INST_B(inst);
        else
            num = INST_AA(inst);
        if(inst_op == OP_EXECUTE_INLINE) {
            vC = FETCH(2) & 0xf;
            vD = (FETCH(2) >> 4) & 0xf;
            vE = (FETCH(2) >> 8) & 0xf;
            vF = FETCH(2) >> 12;
        } else {
            vC = FETCH(2);
            vD = vC + 1;
            vE = vC + 2;
            vF = vC + 3;
        }
        codeSize = 3;
        if(num >= 1) {
            infoArray[0].regNum = vC;
            infoArray[0].refCount = 1;
            infoArray[0].accessType = REGACCESS_U;
            infoArray[0].physicalType = LowOpndRegType_gp;
        }
        if(num >= 2) {
            infoArray[1].regNum = vD;
            infoArray[1].refCount = 1;
            infoArray[1].accessType = REGACCESS_U;
            infoArray[1].physicalType = LowOpndRegType_gp;
        }
        if(num >= 3) {
            infoArray[2].regNum = vE;
            infoArray[2].refCount = 1;
            infoArray[2].accessType = REGACCESS_U;
            infoArray[2].physicalType = LowOpndRegType_gp;
        }
        if(num >= 4) {
            infoArray[3].regNum = vF;
            infoArray[3].refCount = 1;
            infoArray[3].accessType = REGACCESS_U;
            infoArray[3].physicalType = LowOpndRegType_gp;
        }
        updateCurrentBBWithConstraints(PhysicalReg_EAX);
        updateCurrentBBWithConstraints(PhysicalReg_EDX);
        num_regs_per_bytecode = num;
        break;
#if FIXME
    case OP_INVOKE_OBJECT_INIT_RANGE:
        codeSize = 3;
        num_regs_per_bytecode = 0;
        break;
#endif
    }
    return codeSize;
}
//! Updates infoArray(TempRegInfo) with temporaries accessed by INVOKE_NO_RANGE

//!
int updateInvokeNoRange(TempRegInfo* infoArray, int startInd) {
    int j = startInd;
    //invokeMethodNoRange
    int count = INST_B(inst);
    if(count == 5) {
        infoArray[j].regNum = 22;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count >= 4) {
        infoArray[j].regNum = 23;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count >= 3) {
        infoArray[j].regNum = 24;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count >= 2) {
        infoArray[j].regNum = 25;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count >= 1) {
        infoArray[j].regNum = 26;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    return j;
}
//! Updates infoArray(TempRegInfo) with temporaries accessed by INVOKE_RANGE

//! LOOP_COUNT is used to indicate a variable is live through a loop
int updateInvokeRange(TempRegInfo* infoArray, int startIndex) {
    int j = startIndex;
    int count = INST_AA(inst);
    infoArray[j].regNum = 21;
    if(count <= 10) {
        infoArray[j].refCount = 1+count; //DU
    } else {
        infoArray[j].refCount = 2+3*LOOP_COUNT;
    }
    infoArray[j].physicalType = LowOpndRegType_gp;
    j++;
    if(count >= 1 && count <= 10) {
        infoArray[j].regNum = 22;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count >= 2 && count <= 10) {
        infoArray[j].regNum = 23;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count >= 3 && count <= 10) {
        infoArray[j].regNum = 24;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count >= 4 && count <= 10) {
        infoArray[j].regNum = 25;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count >= 5 && count <= 10) {
        infoArray[j].regNum = 26;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count >= 6 && count <= 10) {
        infoArray[j].regNum = 27;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count >= 7 && count <= 10) {
        infoArray[j].regNum = 28;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count >= 8 && count <= 10) {
        infoArray[j].regNum = 29;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count >= 9 && count <= 10) {
        infoArray[j].regNum = 30;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count == 10) {
        infoArray[j].regNum = 31;
        infoArray[j].refCount = 2; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    if(count > 10) {
        //NOTE: inside a loop, LOOP_COUNT can't be 1
        //      if LOOP_COUNT is 1, it is likely that a logical register is freed inside the loop
        //         and the next iteration will have incorrect result
        infoArray[j].regNum = 12;
        infoArray[j].refCount = 1+3*LOOP_COUNT; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
        infoArray[j].regNum = 13;
        infoArray[j].refCount = 1+LOOP_COUNT; //DU
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
        infoArray[j].regNum = 14;
        //MUST be 2, otherwise, transferToState will think its state was in memory
        infoArray[j].refCount = 2; //DU local
        infoArray[j].physicalType = LowOpndRegType_gp;
        j++;
    }
    return j;
}

/* update temporaries used by RETURN bytecodes
   a temporary is represented by <number, type of the temporary>
   */
int updateReturnCommon(TempRegInfo* infoArray) {
    int numTmps;
    infoArray[0].regNum = 1;
    infoArray[0].refCount = 4; //DU
    infoArray[0].physicalType = LowOpndRegType_scratch;
    infoArray[1].regNum = 2;
    infoArray[1].refCount = 2; //DU
    infoArray[1].physicalType = LowOpndRegType_scratch;
    infoArray[2].regNum = PhysicalReg_EAX;
    infoArray[2].refCount = 5; //DU
    infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

    infoArray[3].regNum = 1;
#if defined(ENABLE_TRACING)//WITH_DEBUGGER is true WITH_PROFILER can be false
    infoArray[3].refCount = 6+4;
#else
    infoArray[3].refCount = 6; //DU
#endif
    infoArray[3].physicalType = LowOpndRegType_gp;
    infoArray[4].regNum = 2;
    infoArray[4].refCount = 4; //DU
    infoArray[4].physicalType = LowOpndRegType_gp;
    infoArray[5].regNum = 5;
    infoArray[5].refCount = 2; //DU
    infoArray[5].physicalType = LowOpndRegType_gp;
    infoArray[6].regNum = 10;
    infoArray[6].refCount = 3;
    infoArray[6].physicalType = LowOpndRegType_gp;
    infoArray[7].regNum = 6;
    infoArray[7].refCount = 4; //DU
    infoArray[7].physicalType = LowOpndRegType_gp;
    infoArray[8].regNum = 3;
    infoArray[8].refCount = 3;
    infoArray[8].physicalType = LowOpndRegType_gp;
    infoArray[9].regNum = 7;
    infoArray[9].refCount = 2; //DU
    infoArray[9].physicalType = LowOpndRegType_gp;
    numTmps = 12;
#if defined(ENABLE_TRACING)
    infoArray[12].regNum = 4;
    infoArray[12].refCount = 3; //DU
    infoArray[12].physicalType = LowOpndRegType_gp;
    infoArray[13].regNum = 3;
    infoArray[13].refCount = 2; //DU
    infoArray[13].physicalType = LowOpndRegType_scratch;
    infoArray[14].regNum = 15;
    infoArray[14].refCount = 2; //DU
    infoArray[14].physicalType = LowOpndRegType_gp;
    infoArray[15].regNum = 16;
    infoArray[15].refCount = 2; //DU
    infoArray[15].physicalType = LowOpndRegType_gp;
    infoArray[16].regNum = PhysicalReg_EDX;
    infoArray[16].refCount = 2; //DU
    infoArray[16].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
    infoArray[17].regNum = 6;
    infoArray[17].refCount = 2; //DU
    infoArray[17].physicalType = LowOpndRegType_scratch;
    numTmps = 18;
#endif
    infoArray[10].regNum = 14;
    infoArray[10].refCount = 2; //DU
    infoArray[10].physicalType = LowOpndRegType_gp;
    infoArray[11].regNum = 4;
    infoArray[11].refCount = 2; //DU
    infoArray[11].physicalType = LowOpndRegType_scratch;
#ifdef DEBUG_CALL_STACK
    infoArray[numTmps].regNum = 5;
    infoArray[numTmps].refCount = 2;
    infoArray[numTmps].physicalType = LowOpndRegType_scratch;
    numTmps++;
#endif
    infoArray[numTmps].regNum = PhysicalReg_EBX;
    /* used to hold chaining cell
       updated to be returnAddr
       then conditionally updated to zero
       used to update inJitCodeCache
       compare against zero to determine whether to jump to native code
       jump to native code (%ebx)
    */
    infoArray[numTmps].refCount = 3+1+1;
    infoArray[numTmps].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
    numTmps++;
    infoArray[numTmps].regNum = 17;
    infoArray[numTmps].refCount = 2; //DU
    infoArray[numTmps].physicalType = LowOpndRegType_gp;
    numTmps++;
    infoArray[numTmps].regNum = 7;
    infoArray[numTmps].refCount = 4; //DU
    infoArray[numTmps].physicalType = LowOpndRegType_scratch;
    numTmps++;
    return numTmps;
}

/* update temporaries used by predicted INVOKE_VIRTUAL & INVOKE_INTERFACE */
int updateGenPrediction(TempRegInfo* infoArray, bool isInterface) {
    infoArray[0].regNum = 40;
    infoArray[0].physicalType = LowOpndRegType_gp;
    infoArray[1].regNum = 41;
    infoArray[1].physicalType = LowOpndRegType_gp;
    infoArray[2].regNum = 32;
    infoArray[2].refCount = 2;
    infoArray[2].physicalType = LowOpndRegType_gp;

    if(isInterface) {
        infoArray[0].refCount = 2+2;
        infoArray[1].refCount = 3+2-1; //for temp41, -1 for gingerbread
        infoArray[3].regNum = 33;
        infoArray[3].refCount = 4+1;
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = PhysicalReg_EAX;
        infoArray[4].refCount = 5;
        infoArray[4].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[5].regNum = PhysicalReg_ECX;
        infoArray[5].refCount = 1+1+2; //used in ArgsDone (twice)
        infoArray[5].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[6].regNum = 10;
        infoArray[6].refCount = 2;
        infoArray[6].physicalType = LowOpndRegType_scratch;
        infoArray[7].regNum = 9;
        infoArray[7].refCount = 2;
        infoArray[7].physicalType = LowOpndRegType_scratch;
        infoArray[8].regNum = 8;
        infoArray[8].refCount = 2;
        infoArray[8].physicalType = LowOpndRegType_scratch;
        infoArray[9].regNum = PhysicalReg_EDX; //space holder
        infoArray[9].refCount = 1;
        infoArray[9].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[10].regNum = 43;
        infoArray[10].refCount = 3;
        infoArray[10].physicalType = LowOpndRegType_gp;
        infoArray[11].regNum = 44;
        infoArray[11].refCount = 3;
        infoArray[11].physicalType = LowOpndRegType_gp;
        infoArray[12].regNum = 45;
        infoArray[12].refCount = 2;
        infoArray[12].physicalType = LowOpndRegType_gp;
        infoArray[13].regNum = 7;
        infoArray[13].refCount = 4;
        infoArray[13].physicalType = LowOpndRegType_scratch;
        return 14;
    } else { //virtual or virtual_quick
        infoArray[0].refCount = 2+2;
        infoArray[1].refCount = 3+2-2; //for temp41, -2 for gingerbread
        infoArray[2].refCount++; //for temp32 gingerbread
        infoArray[3].regNum = 33;
        infoArray[3].refCount = 4+1;
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 34;
        infoArray[4].refCount = 2;
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[5].regNum = PhysicalReg_EAX;
        infoArray[5].refCount = 2;
        infoArray[5].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[6].regNum = PhysicalReg_ECX;
        infoArray[6].refCount = 1+3+2;
        infoArray[6].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[7].regNum = 10;
        infoArray[7].refCount = 2;
        infoArray[7].physicalType = LowOpndRegType_scratch;
        infoArray[8].regNum = PhysicalReg_EDX; //space holder
        infoArray[8].refCount = 1;
        infoArray[8].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[9].regNum = 43;
        infoArray[9].refCount = 3;
        infoArray[9].physicalType = LowOpndRegType_gp;
        infoArray[10].regNum = 44;
        infoArray[10].refCount = 3;
        infoArray[10].physicalType = LowOpndRegType_gp;
        infoArray[11].regNum = 7;
        infoArray[11].refCount = 4;
        infoArray[11].physicalType = LowOpndRegType_scratch;
        return 12;
    }
}

int updateMarkCard(TempRegInfo* infoArray, int j1/*valReg*/,
                    int j2/*tgtAddrReg*/, int j3/*scratchReg*/) {
    infoArray[j3].regNum = 11;
    infoArray[j3].physicalType = LowOpndRegType_gp;
    infoArray[j3].refCount = 3;
    infoArray[j3].is8Bit = true;
    infoArray[j1].refCount++;
    infoArray[j2].refCount += 2;
    infoArray[j3+1].regNum = 6;
    infoArray[j3+1].physicalType = LowOpndRegType_scratch;
    infoArray[j3+1].refCount = 2;
    return j3+2;
}

int updateMarkCard_notNull(TempRegInfo* infoArray,
                           int j2/*tgtAddrReg*/, int j3/*scratchReg*/) {
    infoArray[j3].regNum = 11;
    infoArray[j3].physicalType = LowOpndRegType_gp;
    infoArray[j3].refCount = 3;
    infoArray[j3].is8Bit = true;
    infoArray[j2].refCount += 2;
    infoArray[j3+1].regNum = 2;
    infoArray[j3+1].refCount = 2; //DU
    infoArray[j3+1].physicalType = LowOpndRegType_scratch;
    return j3+2;
}

int iget_obj_inst = -1;
//! This function updates infoArray with temporaries accessed when lowering the bytecode

//! returns the number of temporaries
int getTempRegInfo(TempRegInfo* infoArray) { //returns an array of TempRegInfo
    int k;
    int numTmps;
    for(k = 0; k < MAX_TEMP_REG_PER_BYTECODE; k++) {
        infoArray[k].linkageToVR = -1;
        infoArray[k].versionNum = 0;
        infoArray[k].shareWithVR = true;
        infoArray[k].is8Bit = false;
    }
    u2 vA, v1, length, num, tmp;
    u2 inst_op = INST_INST(inst);
    s2 tmp_s2;
    s4 tmp_s4;
    switch(inst_op) {
    case OP_APUT_BYTE:
        for(k = 0; k < MAX_TEMP_REG_PER_BYTECODE; k++)
            infoArray[k].shareWithVR = true; //false;
        break;
    }
    switch (INST_INST(inst)) {
    case OP_NOP:
        return 0;
    case OP_MOVE:
    case OP_MOVE_OBJECT:
    case OP_MOVE_FROM16:
    case OP_MOVE_OBJECT_FROM16:
    case OP_MOVE_16:
    case OP_MOVE_OBJECT_16:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        return 1;
    case OP_MOVE_WIDE:
    case OP_MOVE_WIDE_FROM16:
    case OP_MOVE_WIDE_16:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_xmm;
        return 1;
    case OP_MOVE_RESULT:
    case OP_MOVE_RESULT_OBJECT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 1;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_scratch;
        return 2;
    case OP_MOVE_RESULT_WIDE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = 1;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_scratch;
        return 2;
    case OP_MOVE_EXCEPTION:
        infoArray[0].regNum = 2;
        infoArray[0].refCount = 3; //DUU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 3;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 1;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_scratch;
        return 3;

    case OP_CONST_4:
    case OP_CONST_16:
    case OP_CONST:
    case OP_CONST_HIGH16:
    case OP_CONST_WIDE_16:
    case OP_CONST_WIDE_32:
    case OP_CONST_WIDE:
    case OP_CONST_WIDE_HIGH16:
        return 0;
    case OP_CONST_STRING: //hardcode %eax
    case OP_CONST_STRING_JUMBO:
        infoArray[0].regNum = 3;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 1;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_scratch;
        infoArray[2].regNum = 2;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_scratch;
        infoArray[3].regNum = PhysicalReg_EAX;
        infoArray[3].refCount = 4;
        infoArray[3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 4;
    case OP_CONST_CLASS:
        infoArray[0].regNum = 3;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 1;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_scratch;
        infoArray[2].regNum = 2;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_scratch;
        infoArray[3].regNum = PhysicalReg_EAX;
        infoArray[3].refCount = 4;
        infoArray[3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 4;

    case OP_MONITOR_ENTER:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 3;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 1;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_scratch;
        infoArray[3].regNum = 2;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_scratch;
        infoArray[4].regNum = PhysicalReg_EDX;
        infoArray[4].refCount = 2;
        infoArray[4].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 5;
    case OP_MONITOR_EXIT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = PhysicalReg_EAX;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[2].regNum = 1;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_scratch;
        infoArray[3].regNum = PhysicalReg_EDX;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[4].regNum = 2;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_scratch;
        infoArray[5].regNum = 3;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_scratch;
        return 6;
    case OP_CHECK_CAST:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 4; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 4;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 6;
        infoArray[2].refCount = 3; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;

        infoArray[3].regNum = 1;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_scratch;
        infoArray[4].regNum = 2;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_scratch;

        infoArray[5].regNum = PhysicalReg_EAX;
        /* %eax has 3 live ranges
           1> 5 accesses: to resolve the class object
           2> call dvmInstanceofNonTrivial to define %eax, then use it once
           3> move exception object to %eax, then jump to throw_exception
           if WITH_JIT is true, the first live range has 6 accesses
        */
        infoArray[5].refCount = 6;
        infoArray[5].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[6].regNum = PhysicalReg_EDX;
        infoArray[6].refCount = 2; //export_pc
        infoArray[6].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[7].regNum = PhysicalReg_ECX;
        infoArray[7].refCount = 1;
        infoArray[7].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[8].regNum = 3;
        infoArray[8].refCount = 2; //DU
        infoArray[8].physicalType = LowOpndRegType_scratch;
        return 9;
    case OP_INSTANCE_OF:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 4; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 3;
        infoArray[1].refCount = 4; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 4;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 6;
        infoArray[3].refCount = 3; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;

        infoArray[4].regNum = 1;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_scratch;
        infoArray[5].regNum = 2;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_scratch;

        infoArray[6].regNum = PhysicalReg_EAX;
        infoArray[6].refCount = 6;
        infoArray[6].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[7].regNum = 3;
        infoArray[7].refCount = 2; //DU
        infoArray[7].physicalType = LowOpndRegType_scratch;
        infoArray[8].regNum = PhysicalReg_EDX;
        infoArray[8].refCount = 2; //export_pc for class_resolve
        infoArray[8].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 9;

    case OP_ARRAY_LENGTH:
        vA = INST_A(inst);
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[1].linkageToVR = vA;
        infoArray[2].regNum = PhysicalReg_EDX;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 3;
    case OP_NEW_INSTANCE:
        infoArray[0].regNum = PhysicalReg_EAX;
        //6: class object
        //3: defined by C function, used twice
        infoArray[0].refCount = 6; //next version has 3 references
        infoArray[0].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[1].regNum = PhysicalReg_ECX; //before common_throw_message
        infoArray[1].refCount = 1;
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 5;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[3].is8Bit = true;
        infoArray[4].regNum = 6;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_gp;

        infoArray[5].regNum = 1;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_scratch;
        infoArray[6].regNum = 2;
        infoArray[6].refCount = 2; //DU
        infoArray[6].physicalType = LowOpndRegType_scratch;
        infoArray[7].regNum = 3;
        infoArray[7].refCount = 2; //DU
        infoArray[7].physicalType = LowOpndRegType_scratch;

        infoArray[8].regNum = PhysicalReg_EDX; //before common_throw_message
        infoArray[8].refCount = 2;
        infoArray[8].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[9].regNum = 4;
        infoArray[9].refCount = 2; //DU
        infoArray[9].physicalType = LowOpndRegType_scratch;
        return 10;

    case OP_NEW_ARRAY:
        infoArray[0].regNum = PhysicalReg_EAX;
        //4: class object
        //3: defined by C function, used twice
        infoArray[0].refCount = 4; //next version has 3 references
        infoArray[0].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[1].regNum = PhysicalReg_EDX; //before common_throw_message
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 5;
        infoArray[3].refCount = 3; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;

        infoArray[4].regNum = 1;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_scratch;
        infoArray[5].regNum = 2;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_scratch;
        infoArray[6].regNum = 3;
        infoArray[6].refCount = 2; //DU
        infoArray[6].physicalType = LowOpndRegType_scratch;
        infoArray[7].regNum = 4;
        infoArray[7].refCount = 2; //DU
        infoArray[7].physicalType = LowOpndRegType_scratch;
        return 8;

    case OP_FILLED_NEW_ARRAY:
        length = INST_B(inst);
        infoArray[0].regNum = PhysicalReg_EAX;
        //4: class object
        //3: defined by C function, used twice (array object)
        //length: access array object to update the content
        infoArray[0].refCount = 4; //next version has 5+length references
        infoArray[0].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[1].regNum = PhysicalReg_EDX; //before common_throw_message
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 5;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 6;
        infoArray[4].refCount = 8; //DU
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[4].is8Bit = true;

        if(length >= 1) {
            infoArray[5].regNum = 7;
            infoArray[5].refCount = 2; //DU
            infoArray[5].physicalType = LowOpndRegType_gp;
        }
        if(length >= 2) {
            infoArray[6].regNum = 8;
            infoArray[6].refCount = 2; //DU
            infoArray[6].physicalType = LowOpndRegType_gp;
        }
        if(length >= 3) {
            infoArray[7].regNum = 9;
            infoArray[7].refCount = 2; //DU
            infoArray[7].physicalType = LowOpndRegType_gp;
        }
        if(length >= 4) {
            infoArray[8].regNum = 10;
            infoArray[8].refCount = 2; //DU
            infoArray[8].physicalType = LowOpndRegType_gp;
        }
        if(length >= 5) {
            infoArray[9].regNum = 11;
            infoArray[9].refCount = 2; //DU
            infoArray[9].physicalType = LowOpndRegType_gp;
        }
        infoArray[5+length].regNum = 1;
        infoArray[5+length].refCount = 2; //DU
        infoArray[5+length].physicalType = LowOpndRegType_scratch;
        infoArray[6+length].regNum = 2;
        infoArray[6+length].refCount = 4; //DU
        infoArray[6+length].physicalType = LowOpndRegType_scratch;
        infoArray[7+length].regNum = 3;
        infoArray[7+length].refCount = 2; //DU
        infoArray[7+length].physicalType = LowOpndRegType_scratch;
        infoArray[8+length].regNum = 4;
        infoArray[8+length].refCount = 5; //DU
        infoArray[8+length].physicalType = LowOpndRegType_scratch;
        return 9+length;

    case OP_FILLED_NEW_ARRAY_RANGE:
        length = INST_AA(inst);
        infoArray[0].regNum = PhysicalReg_EAX;
        //4: class object
        //3: defined by C function, used twice (array object)
        //if length is 0, no access to array object
        //else, used inside a loop
        infoArray[0].refCount = 4; //next version: 5+(length >= 1 ? LOOP_COUNT : 0)
        infoArray[0].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[1].regNum = PhysicalReg_EDX; //before common_throw_message
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 5;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 6;
        infoArray[4].refCount = 8; //DU
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[4].is8Bit = true;

        infoArray[5].regNum = 1;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_scratch;
        infoArray[6].regNum = 2;
        infoArray[6].refCount = 4; //DU
        infoArray[6].physicalType = LowOpndRegType_scratch;
        infoArray[7].regNum = 3;
        infoArray[7].refCount = 2; //DU
        infoArray[7].physicalType = LowOpndRegType_scratch;

        infoArray[8].regNum = 7;
        infoArray[8].refCount = 3*(length >= 1 ? LOOP_COUNT : 0);
        infoArray[8].physicalType = LowOpndRegType_gp;
        infoArray[9].regNum = 8;
        infoArray[9].refCount = 3*(length >= 1 ? LOOP_COUNT : 0);
        infoArray[9].physicalType = LowOpndRegType_gp;
        infoArray[10].regNum = 9;
        infoArray[10].refCount = 2*(length >= 1 ? LOOP_COUNT : 0);
        infoArray[10].physicalType = LowOpndRegType_gp;
        infoArray[11].regNum = 10;
        infoArray[11].refCount = 2*(length >= 1 ? LOOP_COUNT : 0);
        infoArray[11].physicalType = LowOpndRegType_gp;
        infoArray[12].regNum = 4;
        infoArray[12].refCount = 5; //DU
        infoArray[12].physicalType = LowOpndRegType_scratch;
        return 13;

    case OP_FILL_ARRAY_DATA:
        infoArray[0].regNum = PhysicalReg_EAX;
        infoArray[0].refCount = 2;
        infoArray[0].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[1].regNum = PhysicalReg_EDX; //before common_throw_message
#if 0//def HARDREG_OPT
        infoArray[1].refCount = 3; //next version has refCount of 2
#else
        infoArray[1].refCount = 5;
#endif
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[2].regNum =1;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;

        infoArray[3].regNum = 1;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_scratch;
        infoArray[4].regNum = 2;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_scratch;
        return 5;

    case OP_THROW:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = PhysicalReg_EDX; //before common_throw_message
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[2].regNum = 1;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_scratch;
        infoArray[3].regNum = 2;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_scratch;
        return 4;
    case OP_THROW_VERIFICATION_ERROR:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = PhysicalReg_EDX; //export_pc
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[2].regNum = 1;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_scratch;
        infoArray[3].regNum = 2;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_scratch;
        return 4;

    case OP_GOTO: //called function common_periodicChecks4
#if defined(ENABLE_TRACING)
        tt = INST_AA(inst);
        tmp_s2 = (s2)((s2)tt << 8) >> 8;
        if(tmp_s2 < 0) {
            infoArray[0].regNum = PhysicalReg_EDX;
            infoArray[0].refCount = 2;
            infoArray[0].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
            return 1;
        }
#endif
        return 0;
    case OP_GOTO_16:
#if defined(ENABLE_TRACING)
        tmp_s2 = (s2)FETCH(1);
        if(tmp_s2 < 0) {
            infoArray[0].regNum = PhysicalReg_EDX;
            infoArray[0].refCount = 2;
            infoArray[0].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
            return 1;
        }
#endif
        return 0;
    case OP_GOTO_32:
#if defined(ENABLE_TRACING)
        tmp_u4 = (u4)FETCH(1);
        tmp_u4 |= (u4)FETCH(2) << 16;
        if(((s4)tmp_u4) < 0) {
            infoArray[0].regNum = PhysicalReg_EDX;
            infoArray[0].refCount = 2;
            infoArray[0].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
            return 1;
        }
#endif
        return 0;
    case OP_IF_EQ:
    case OP_IF_NE:
    case OP_IF_LT:
    case OP_IF_GE:
    case OP_IF_GT:
    case OP_IF_LE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
#if defined(ENABLE_TRACING)
        tmp_s2 = (s2)FETCH(1);
        if(tmp_s2 < 0) {
            infoArray[1].regNum = PhysicalReg_EDX;
            infoArray[1].refCount = 2;
            infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
            return 2;
        }
#endif
        return 1;
    case OP_IF_EQZ: //called function common_periodicChecks4
    case OP_IF_NEZ:
    case OP_IF_LTZ:
    case OP_IF_GEZ:
    case OP_IF_GTZ:
    case OP_IF_LEZ:
#if defined(ENABLE_TRACING)
        tmp_s2 = (s2)FETCH(1);
        if(tmp_s2 < 0) {
            infoArray[0].regNum = PhysicalReg_EDX;
            infoArray[0].refCount = 2;
            infoArray[0].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
            return 1;
        }
#endif
        return 0;
    case OP_PACKED_SWITCH: //jump common_backwardBranch, which calls common_periodicChecks_entry, then jump_reg %eax
    case OP_SPARSE_SWITCH: //%edx, %eax
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = PhysicalReg_EDX;
        infoArray[1].refCount = 6;
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[2].regNum = PhysicalReg_EAX; //return by dvm helper
        infoArray[2].refCount = 2+1; //2 uses
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[3].regNum = 1;
        infoArray[3].refCount = 2;
        infoArray[3].physicalType = LowOpndRegType_scratch;
        infoArray[4].regNum = 2;
        infoArray[4].refCount = 2;
        infoArray[4].physicalType = LowOpndRegType_scratch;
        return 5;

    case OP_AGET:
    case OP_AGET_OBJECT:
    case OP_AGET_BOOLEAN:
    case OP_AGET_BYTE:
    case OP_AGET_CHAR:
    case OP_AGET_SHORT:
        vA = INST_AA(inst);
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 4; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 3; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 4;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[3].linkageToVR = vA;
        if(inst_op == OP_AGET_BYTE || inst_op == OP_AGET_BOOLEAN)
            infoArray[3].is8Bit = true;
        infoArray[4].regNum = PhysicalReg_EDX;
        infoArray[4].refCount = 2;
        infoArray[4].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 5;
    case OP_AGET_WIDE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 4; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 3; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 1;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_xmm;
        infoArray[4].regNum = PhysicalReg_EDX;
        infoArray[4].refCount = 2;
        infoArray[4].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 5;

    case OP_APUT:
    case OP_APUT_BOOLEAN:
    case OP_APUT_BYTE:
    case OP_APUT_CHAR:
    case OP_APUT_SHORT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 4; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 3; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 4;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        if(inst_op == OP_APUT_BYTE || inst_op == OP_APUT_BOOLEAN)
            infoArray[3].is8Bit = true;
        infoArray[4].regNum = PhysicalReg_EDX;
        infoArray[4].refCount = 2;
        infoArray[4].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 5;
    case OP_APUT_WIDE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 4; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 3; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 1;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_xmm;
        infoArray[4].regNum = PhysicalReg_EDX;
        infoArray[4].refCount = 2;
        infoArray[4].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 5;
    case OP_APUT_OBJECT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 5+1; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2; //live through function call dvmCanPut
        infoArray[1].refCount = 3+1; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 4;
        infoArray[3].refCount = 4+1; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 5;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[5].regNum = 6;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_gp;

        infoArray[6].regNum = PhysicalReg_EDX;
        infoArray[6].refCount = 2; //DU
        infoArray[6].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[7].regNum = PhysicalReg_EAX;
        infoArray[7].refCount = 2; //DU
        infoArray[7].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[8].regNum = 1;
        infoArray[8].refCount = 2; //DU
        infoArray[8].physicalType = LowOpndRegType_scratch;
        infoArray[0].shareWithVR = false;
        return updateMarkCard_notNull(infoArray,
                                      0/*index for tgtAddrReg*/, 9);

    case OP_IGET:
    case OP_IGET_OBJECT:
    case OP_IGET_VOLATILE:
    case OP_IGET_OBJECT_VOLATILE:
    case OP_IGET_BOOLEAN:
    case OP_IGET_BYTE:
    case OP_IGET_CHAR:
    case OP_IGET_SHORT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_scratch;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_scratch;

        infoArray[2].regNum = PhysicalReg_EDX;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[3].regNum = PhysicalReg_EAX;
        infoArray[3].refCount = 3; //DU
        infoArray[3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[4].regNum = 3;
        infoArray[4].refCount = 3; //DU
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[5].regNum = 7;
#ifdef DEBUG_IGET_OBJ
        //add hack for a specific instance (iget_obj_inst) of IGET_OBJECT within a method
        if(inst_op == OP_IGET_OBJECT && !strncmp(currentMethod->clazz->descriptor, "Lspec/benchmarks/_228_jack/Parse", 32) &&
           !strncmp(currentMethod->name, "buildPhase3", 11))
        {
#if 0
          if(iget_obj_inst == 12) {
            LOGD("increase count for instance %d of %s %s", iget_obj_inst, currentMethod->clazz->descriptor, currentMethod->name);
            infoArray[5].refCount = 4; //DU
          }
          else
#endif
            infoArray[5].refCount = 3;
          iget_obj_inst++;
        }
        else
          infoArray[5].refCount = 3;
#else
        infoArray[5].refCount = 3; //DU
#endif
        infoArray[5].physicalType = LowOpndRegType_gp;
        infoArray[6].regNum = 8;
        infoArray[6].refCount = 2; //DU
        infoArray[6].physicalType = LowOpndRegType_gp;
        infoArray[7].regNum = 9;
        infoArray[7].refCount = 2; //DU
        infoArray[7].physicalType = LowOpndRegType_gp;
        return 8;
    case OP_IPUT:
    case OP_IPUT_OBJECT:
    case OP_IPUT_VOLATILE:
    case OP_IPUT_OBJECT_VOLATILE:
    case OP_IPUT_BOOLEAN:
    case OP_IPUT_BYTE:
    case OP_IPUT_CHAR:
    case OP_IPUT_SHORT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_scratch;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_scratch;

        infoArray[2].regNum = PhysicalReg_EDX;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[3].regNum = PhysicalReg_EAX;
        infoArray[3].refCount = 3; //DU
        infoArray[3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[4].regNum = 3;
        infoArray[4].refCount = 3; //DU
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[5].regNum = 7;
        infoArray[5].refCount = 3; //DU
        infoArray[5].physicalType = LowOpndRegType_gp;
        infoArray[6].regNum = 8;
        infoArray[6].refCount = 2; //DU
        infoArray[6].physicalType = LowOpndRegType_gp;
        infoArray[7].regNum = 9;
        infoArray[7].refCount = 2; //DU
        infoArray[7].physicalType = LowOpndRegType_gp;
        if(inst_op == OP_IPUT_OBJECT || inst_op == OP_IPUT_OBJECT_VOLATILE) {
            infoArray[5].shareWithVR = false;
            return updateMarkCard(infoArray, 7/*index for valReg*/,
                                  5/*index for tgtAddrReg*/, 8);
        }
        return 8;
    case OP_IGET_WIDE:
    case OP_IGET_WIDE_VOLATILE:
    case OP_IPUT_WIDE:
    case OP_IPUT_WIDE_VOLATILE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_scratch;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_scratch;

        infoArray[2].regNum = PhysicalReg_EDX;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[3].regNum = PhysicalReg_EAX;
        infoArray[3].refCount = 3; //DU
        infoArray[3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[4].regNum = 3;
        infoArray[4].refCount = 3; //DU
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[5].regNum = 7;
        infoArray[5].refCount = 3; //DU
        infoArray[5].physicalType = LowOpndRegType_gp;
        infoArray[6].regNum = 8;
        infoArray[6].refCount = 2; //DU
        infoArray[6].physicalType = LowOpndRegType_gp;
        infoArray[7].regNum = 1;
        infoArray[7].refCount = 2; //DU
        infoArray[7].physicalType = LowOpndRegType_xmm;

        if(inst_op == OP_IPUT_WIDE_VOLATILE || inst_op == OP_IGET_WIDE_VOLATILE) {
            infoArray[8].regNum = 3;
            infoArray[8].refCount = 2; //DU
            infoArray[8].physicalType = LowOpndRegType_scratch;
            infoArray[9].regNum = 9;
            infoArray[9].refCount = 2; //DU
            infoArray[9].physicalType = LowOpndRegType_gp;
            return 10;
        }
        return 8;

    case OP_SGET:
    case OP_SGET_OBJECT:
    case OP_SGET_VOLATILE:
    case OP_SGET_OBJECT_VOLATILE:
    case OP_SGET_BOOLEAN:
    case OP_SGET_BYTE:
    case OP_SGET_CHAR:
    case OP_SGET_SHORT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_scratch;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_scratch;

        infoArray[2].regNum = PhysicalReg_EAX;
        infoArray[2].refCount = 2;
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[3].regNum = 3;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 7;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[5].regNum = PhysicalReg_EDX;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 6;
    case OP_SPUT:
    case OP_SPUT_OBJECT:
    case OP_SPUT_VOLATILE:
    case OP_SPUT_OBJECT_VOLATILE:
    case OP_SPUT_BOOLEAN:
    case OP_SPUT_BYTE:
    case OP_SPUT_CHAR:
    case OP_SPUT_SHORT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_scratch;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_scratch;

        infoArray[2].regNum = PhysicalReg_EAX;
        infoArray[2].refCount = 2+1; //access clazz of the field
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[3].regNum = 3;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 7;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[5].regNum = PhysicalReg_EDX;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        if(inst_op == OP_SPUT_OBJECT || inst_op == OP_SPUT_OBJECT_VOLATILE) {
            infoArray[2].shareWithVR = false;
            infoArray[6].regNum = 12;
            infoArray[6].refCount = 1; //1 def, 2 uses in updateMarkCard
            infoArray[6].physicalType = LowOpndRegType_gp;
            return updateMarkCard(infoArray, 4/*index for valReg*/,
                                  6/*index for tgtAddrReg */, 7);
        }
        return 6;
    case OP_SGET_WIDE:
    case OP_SGET_WIDE_VOLATILE:
    case OP_SPUT_WIDE:
    case OP_SPUT_WIDE_VOLATILE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_scratch;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_scratch;

        infoArray[2].regNum = PhysicalReg_EAX;
        infoArray[2].refCount = 2;
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[3].regNum = 3;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 1;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_xmm;
        infoArray[5].regNum = PhysicalReg_EDX;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        if(inst_op == OP_SPUT_WIDE_VOLATILE || inst_op == OP_SGET_WIDE_VOLATILE) {
            infoArray[6].regNum = 3;
            infoArray[6].refCount = 2; //DU
            infoArray[6].physicalType = LowOpndRegType_scratch;
            infoArray[7].regNum = 9;
            infoArray[7].refCount = 2; //DU
            infoArray[7].physicalType = LowOpndRegType_gp;
            return 8;
        }
        return 6;

    case OP_IGET_QUICK:
    case OP_IGET_OBJECT_QUICK:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = PhysicalReg_EDX;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 3;
    case OP_IPUT_QUICK:
    case OP_IPUT_OBJECT_QUICK:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = PhysicalReg_EDX;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        if(inst_op == OP_IPUT_OBJECT_QUICK) {
            infoArray[0].shareWithVR = false;
            return updateMarkCard(infoArray, 1/*index for valReg*/,
                                  0/*index for tgtAddrReg*/, 3);
        }
        return 3;
    case OP_IGET_WIDE_QUICK:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 1;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_xmm;
        infoArray[2].regNum = PhysicalReg_EDX;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 3;
    case OP_IPUT_WIDE_QUICK:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 1;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_xmm;
        infoArray[2].regNum = PhysicalReg_EDX;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 3;

    case OP_RETURN_VOID:
    case OP_RETURN_VOID_BARRIER:
        return updateReturnCommon(infoArray);
    case OP_RETURN:
    case OP_RETURN_OBJECT:
        numTmps = updateReturnCommon(infoArray);

        infoArray[numTmps].regNum = 21;
        infoArray[numTmps].refCount = 2; //DU
        infoArray[numTmps].physicalType = LowOpndRegType_gp;
        numTmps++;
        infoArray[numTmps].regNum = 22;
        infoArray[numTmps].refCount = 2; //DU
        infoArray[numTmps].physicalType = LowOpndRegType_gp;
        numTmps++;
        return numTmps;
    case OP_RETURN_WIDE:
        numTmps = updateReturnCommon(infoArray);

        infoArray[numTmps].regNum = 10;
        infoArray[numTmps].refCount = 2; //DU
        infoArray[numTmps].physicalType = LowOpndRegType_scratch;
        numTmps++;
        infoArray[numTmps].regNum = 1;
        infoArray[numTmps].refCount = 2; //DU
        infoArray[numTmps].physicalType = LowOpndRegType_xmm;
        numTmps++;
        return numTmps;

    case OP_INVOKE_VIRTUAL:
    case OP_INVOKE_VIRTUAL_RANGE:
#ifdef PREDICTED_CHAINING
        numTmps = updateGenPrediction(infoArray, false /*not interface*/);
        infoArray[numTmps].regNum = 5;
        infoArray[numTmps].refCount = 3; //DU
        infoArray[numTmps].physicalType = LowOpndRegType_gp;
        numTmps++;
        if(inst_op == OP_INVOKE_VIRTUAL)
            k = updateInvokeNoRange(infoArray, numTmps);
        else
            k = updateInvokeRange(infoArray, numTmps);
        return k;
#else
        infoArray[0].regNum = 3;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 7;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 8;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 6;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 5;
        infoArray[4].refCount = 3; //DU
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[5].regNum = PhysicalReg_EDX;
        infoArray[5].refCount = 2; //2 versions, first version DU is for exception, 2nd version: eip right before jumping to invokeArgsDone
        infoArray[5].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[6].regNum = PhysicalReg_ECX; //ecx is ued in invokeArgsDone
        infoArray[6].refCount = 1+1; //used in .invokeArgsDone
        infoArray[6].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        //when WITH_JIT is true and PREDICTED_CHAINING is false
        //  temp 8 and EAX are not used; but it is okay to keep it here
        infoArray[7].regNum = PhysicalReg_EAX;
        infoArray[7].refCount = 4; //DU
        infoArray[7].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[8].regNum = 1;
        infoArray[8].refCount = 2; //DU
        infoArray[8].physicalType = LowOpndRegType_scratch;
        infoArray[9].regNum = 2;
        infoArray[9].refCount = 2; //DU
        infoArray[9].physicalType = LowOpndRegType_scratch;
        if(inst_op == OP_INVOKE_VIRTUAL)
            k = updateInvokeNoRange(infoArray, 10);
        else
            k = updateInvokeRange(infoArray, 10);
        return k;
#endif
    case OP_INVOKE_SUPER:
    case OP_INVOKE_SUPER_RANGE:
        infoArray[0].regNum = 3;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 7;
        infoArray[1].refCount = 3; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 8;
        infoArray[2].refCount = 3; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 6;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 9;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_gp;

        infoArray[5].regNum = PhysicalReg_EDX;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[6].regNum = PhysicalReg_ECX;
        infoArray[6].refCount = 1+1; //DU
        infoArray[6].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[7].regNum = PhysicalReg_EAX;
        infoArray[7].refCount = 4; //DU
        infoArray[7].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[8].regNum = 1;
        infoArray[8].refCount = 2; //DU
        infoArray[8].physicalType = LowOpndRegType_scratch;
        infoArray[9].regNum = 2;
        infoArray[9].refCount = 2; //DU
        infoArray[9].physicalType = LowOpndRegType_scratch;
        infoArray[10].regNum = 3;
        infoArray[10].refCount = 2; //DU
        infoArray[10].physicalType = LowOpndRegType_scratch;
        infoArray[11].regNum = 4;
        infoArray[11].refCount = 2; //DU
        infoArray[11].physicalType = LowOpndRegType_scratch;
        if(inst_op == OP_INVOKE_SUPER)
            k = updateInvokeNoRange(infoArray, 12);
        else
            k = updateInvokeRange(infoArray, 12);
        return k;
    case OP_INVOKE_DIRECT:
    case OP_INVOKE_DIRECT_RANGE:
        infoArray[0].regNum = 3;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 5;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;

        infoArray[2].regNum = PhysicalReg_EDX;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[3].regNum = PhysicalReg_ECX;
        infoArray[3].refCount = 2;
        infoArray[3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[4].regNum = PhysicalReg_EAX;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[5].regNum = 1;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_scratch;
        infoArray[6].regNum = 2;
        infoArray[6].refCount = 2; //DU
        infoArray[6].physicalType = LowOpndRegType_scratch;
        if(inst_op == OP_INVOKE_DIRECT)
            k = updateInvokeNoRange(infoArray, 7);
        else
            k = updateInvokeRange(infoArray, 7);
        return k;
    case OP_INVOKE_STATIC:
    case OP_INVOKE_STATIC_RANGE:
        infoArray[0].regNum = 3;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;

        infoArray[1].regNum = PhysicalReg_EDX;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[2].regNum = PhysicalReg_ECX;
        infoArray[2].refCount = 2;
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[3].regNum = PhysicalReg_EAX;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[4].regNum = 1;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_scratch;
        infoArray[5].regNum = 2;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_scratch;
        if(inst_op == OP_INVOKE_STATIC)
            k = updateInvokeNoRange(infoArray, 6);
        else
            k = updateInvokeRange(infoArray, 6);
        return k;
    case OP_INVOKE_INTERFACE:
    case OP_INVOKE_INTERFACE_RANGE:
#ifdef PREDICTED_CHAINING
        numTmps = updateGenPrediction(infoArray, true /*interface*/);
        infoArray[numTmps].regNum = 1;
        infoArray[numTmps].refCount = 3; //DU
        infoArray[numTmps].physicalType = LowOpndRegType_gp;
        numTmps++;
        if(inst_op == OP_INVOKE_INTERFACE)
            k = updateInvokeNoRange(infoArray, numTmps);
        else
            k = updateInvokeRange(infoArray, numTmps);
        return k;
#else
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 3;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 4;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 5;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;

        infoArray[4].regNum = PhysicalReg_EDX;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[5].regNum = PhysicalReg_ECX;
        infoArray[5].refCount = 1+1; //DU
        infoArray[5].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[6].regNum = PhysicalReg_EAX;
        infoArray[6].refCount = 2+1; //2 uses
        infoArray[6].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[7].regNum = 1;
        infoArray[7].refCount = 2; //DU
        infoArray[7].physicalType = LowOpndRegType_scratch;
        infoArray[8].regNum = 2;
        infoArray[8].refCount = 2; //DU
        infoArray[8].physicalType = LowOpndRegType_scratch;
        infoArray[9].regNum = 3;
        infoArray[9].refCount = 2; //DU
        infoArray[9].physicalType = LowOpndRegType_scratch;
        if(inst_op == OP_INVOKE_INTERFACE)
            k = updateInvokeNoRange(infoArray, 10);
        else
            k = updateInvokeRange(infoArray, 10);
        return k;
#endif
        ////////////////////////////////////////////// ALU
    case OP_NEG_INT:
    case OP_NOT_INT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //define, update, use
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[0].shareWithVR = false;
        return 1;
    case OP_NEG_LONG:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //define, use
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 4; //define, update, use
        infoArray[1].physicalType = LowOpndRegType_xmm;
        return 2;
    case OP_NOT_LONG:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //define, update, use
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[0].shareWithVR = false;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_xmm;
        return 2;
    case OP_NEG_FLOAT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //define, update, use
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[0].shareWithVR = false;
        return 1;
    case OP_NEG_DOUBLE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //define, use
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 3; //define, update, use
        infoArray[1].physicalType = LowOpndRegType_xmm;
        return 2;
    case OP_INT_TO_LONG: //hard-code eax & edx
        infoArray[0].regNum = PhysicalReg_EAX;
        infoArray[0].refCount = 2+1;
        infoArray[0].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[0].shareWithVR = false;
        infoArray[1].regNum = PhysicalReg_EDX;
        infoArray[1].refCount = 1+1; //cdq accesses edx & eax
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 2;
    case OP_INT_TO_FLOAT:
    case OP_INT_TO_DOUBLE:
    case OP_LONG_TO_FLOAT:
    case OP_LONG_TO_DOUBLE:
    case OP_FLOAT_TO_DOUBLE:
    case OP_DOUBLE_TO_FLOAT:
        return 0; //fp stack
    case OP_LONG_TO_INT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2;
        infoArray[0].physicalType = LowOpndRegType_gp;
        return 1;
    case OP_FLOAT_TO_INT:
    case OP_DOUBLE_TO_INT: //fp stack
        return 0;
    case OP_FLOAT_TO_LONG:
    case OP_DOUBLE_TO_LONG:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //define, use
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //define, use
        infoArray[1].physicalType = LowOpndRegType_xmm;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //define, use
        infoArray[2].physicalType = LowOpndRegType_xmm;
        return 3;
    case OP_INT_TO_BYTE:
    case OP_INT_TO_CHAR:
    case OP_INT_TO_SHORT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 4; //define, update, update, use
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[0].shareWithVR = false;
        return 1;

    case OP_ADD_INT:
    case OP_SUB_INT:
    case OP_MUL_INT:
    case OP_AND_INT:
    case OP_OR_INT:
    case OP_XOR_INT:
    case OP_ADD_INT_2ADDR:
    case OP_SUB_INT_2ADDR:
    case OP_MUL_INT_2ADDR:
    case OP_AND_INT_2ADDR:
    case OP_OR_INT_2ADDR:
    case OP_XOR_INT_2ADDR:
        if(inst_op == OP_ADD_INT || inst_op == OP_SUB_INT || inst_op == OP_MUL_INT ||
           inst_op == OP_AND_INT || inst_op == OP_OR_INT || inst_op == OP_XOR_INT) {
            vA = INST_AA(inst);
            v1 = *((u1*)rPC + 2);
        } else {
            vA = INST_A(inst);
            v1 = vA;
        }
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //define, update, use
        infoArray[0].physicalType = LowOpndRegType_gp;
        if(vA != v1)
            infoArray[0].shareWithVR = false;
        return 1; //common_alu_int

    case OP_SHL_INT:
    case OP_SHR_INT:
    case OP_USHR_INT:
    case OP_SHL_INT_2ADDR:
    case OP_SHR_INT_2ADDR:
    case OP_USHR_INT_2ADDR: //use %cl or %ecx?
        if(inst_op == OP_SHL_INT || inst_op == OP_SHR_INT || inst_op == OP_USHR_INT) {
            vA = INST_AA(inst);
            v1 = *((u1*)rPC + 2);
        } else {
            vA = INST_A(inst);
            v1 = vA;
        }
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //define, update, use
        infoArray[0].physicalType = LowOpndRegType_gp;
        if(vA != v1)
            infoArray[0].shareWithVR = false;
        infoArray[1].regNum = PhysicalReg_ECX;
        infoArray[1].refCount = 2; //define, use
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 2;//common_shift_int

    case OP_DIV_INT:
    case OP_REM_INT:
    case OP_DIV_INT_2ADDR:
    case OP_REM_INT_2ADDR: //hard-code %eax, %edx (dividend in edx:eax; quotient in eax; remainder in edx)
        infoArray[0].regNum = 2;
        infoArray[0].refCount = 4; //define, update, use
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = PhysicalReg_EAX; //dividend, quotient
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[1].shareWithVR = false;
        infoArray[2].regNum = PhysicalReg_EDX; //export_pc, output for REM
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[3].regNum = 1;
        infoArray[3].refCount = 2; //define, use
        infoArray[3].physicalType = LowOpndRegType_scratch;
        if(inst_op == OP_DIV_INT || inst_op == OP_DIV_INT_2ADDR) {
            infoArray[1].refCount = 5;
            infoArray[2].refCount = 4;
        } else {
            infoArray[1].refCount = 4;
            infoArray[2].refCount = 5;
        }
        return 4;

    case OP_ADD_INT_LIT16:
    case OP_MUL_INT_LIT16:
    case OP_AND_INT_LIT16:
    case OP_OR_INT_LIT16:
    case OP_XOR_INT_LIT16:
    case OP_ADD_INT_LIT8:
    case OP_MUL_INT_LIT8:
    case OP_AND_INT_LIT8:
    case OP_OR_INT_LIT8:
    case OP_XOR_INT_LIT8:
    case OP_SHL_INT_LIT8:
    case OP_SHR_INT_LIT8:
    case OP_USHR_INT_LIT8:
        if(inst_op == OP_ADD_INT_LIT16 || inst_op == OP_MUL_INT_LIT16 ||
           inst_op == OP_AND_INT_LIT16 || inst_op == OP_OR_INT_LIT16 || inst_op == OP_XOR_INT_LIT16) {
            vA = INST_A(inst);
            v1 = INST_B(inst);
        } else {
            vA = INST_AA(inst);
            v1 = (u2)FETCH(1) & 0xff;
        }
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //define, update, use
        infoArray[0].physicalType = LowOpndRegType_gp;
        if(vA != v1)
            infoArray[0].shareWithVR = false;
        return 1;

    case OP_RSUB_INT_LIT8:
    case OP_RSUB_INT:
        vA = INST_AA(inst);
        v1 = (inst_op == OP_RSUB_INT) ? INST_B(inst) : ((u2)FETCH(1) & 0xff);
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2;
        infoArray[0].physicalType = LowOpndRegType_gp;
        if(vA != v1)
            infoArray[0].shareWithVR = false;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 3;
        infoArray[1].physicalType = LowOpndRegType_gp;
        if(vA != v1)
            infoArray[1].shareWithVR = false;
        return 2;

    case OP_DIV_INT_LIT16:
    case OP_REM_INT_LIT16:
    case OP_DIV_INT_LIT8:
    case OP_REM_INT_LIT8:
        if(inst_op == OP_DIV_INT_LIT8 || inst_op == OP_REM_INT_LIT8) {
            tmp_s2 = (s2)FETCH(1) >> 8;
        }
        else {
            tmp_s4 = (s2)FETCH(1);
            tmp_s2 = tmp_s4;
        }
        if((inst_op == OP_DIV_INT_LIT8 || inst_op == OP_DIV_INT_LIT16)) {
            int power = isPowerOfTwo(tmp_s2);
            if(power >= 1) { /* divide by a power of 2 constant */
                infoArray[0].regNum = 2;
                infoArray[0].refCount = 3; //define, use, use
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 1;
                infoArray[1].physicalType = LowOpndRegType_gp;
                if(power == 1) infoArray[1].refCount = 5;
                else infoArray[1].refCount = 6;
                return 2;
            }
        }
        if(tmp_s2 == 0) {
            //export_pc
            infoArray[0].regNum = PhysicalReg_EDX; //export_pc, output for REM
            infoArray[0].refCount = 2;
            infoArray[0].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
            return 1;
        }
        if(inst_op == OP_DIV_INT_LIT16 || inst_op == OP_DIV_INT_LIT8) {
            if(tmp_s2 == -1)
                infoArray[1].refCount = 4+1;
            else
                infoArray[1].refCount = 4;
            infoArray[2].refCount = 2; //edx
        } else {
            if(tmp_s2 == -1)
                infoArray[1].refCount = 3+1;
            else
                infoArray[1].refCount = 3;
            infoArray[2].refCount = 3; //edx
        }
        infoArray[0].regNum = 2;
        infoArray[0].refCount = 2; //define, use
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = PhysicalReg_EAX; //dividend, quotient
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[1].shareWithVR = false;
        infoArray[2].regNum = PhysicalReg_EDX; //export_pc, output for REM
        infoArray[2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 3;

    case OP_ADD_LONG:
    case OP_SUB_LONG:
    case OP_AND_LONG:
    case OP_OR_LONG:
    case OP_XOR_LONG:
    case OP_ADD_LONG_2ADDR:
    case OP_SUB_LONG_2ADDR:
    case OP_AND_LONG_2ADDR:
    case OP_OR_LONG_2ADDR:
    case OP_XOR_LONG_2ADDR:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //define, update, use
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[0].shareWithVR = false;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //define, use
        infoArray[1].physicalType = LowOpndRegType_xmm;
        return 2;

    case OP_SHL_LONG:
    case OP_SHL_LONG_2ADDR:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //define, update, use
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[0].shareWithVR = false;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 3; //define, update, use
        infoArray[1].physicalType = LowOpndRegType_xmm;
        infoArray[1].shareWithVR = false;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //define, use
        infoArray[2].physicalType = LowOpndRegType_xmm;
        return 3;

    case OP_SHR_LONG:
    case OP_SHR_LONG_2ADDR:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 4; //define, update, use
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[0].shareWithVR = false;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 4; //define, update, use
        infoArray[1].physicalType = LowOpndRegType_xmm;
        infoArray[1].shareWithVR = false;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //define, use
        infoArray[2].physicalType = LowOpndRegType_xmm;
        infoArray[3].regNum = 4;
        infoArray[3].refCount = 3;
        infoArray[3].physicalType = LowOpndRegType_xmm;
        infoArray[4].regNum = 5;
        infoArray[4].refCount = 3;
        infoArray[4].physicalType = LowOpndRegType_xmm;
        return 5;

    case OP_USHR_LONG:
    case OP_USHR_LONG_2ADDR:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //define, update, use
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[0].shareWithVR = false;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 3; //define, update, use
        infoArray[1].physicalType = LowOpndRegType_xmm;
        infoArray[1].shareWithVR = false;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //define, use
        infoArray[2].physicalType = LowOpndRegType_xmm;
        return 3;

    case OP_MUL_LONG: //general purpose register
    case OP_MUL_LONG_2ADDR:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 6;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[0].shareWithVR = false;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 3;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 3;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = PhysicalReg_EAX;
        infoArray[3].refCount = 2+1; //for mul_opc
        infoArray[3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[4].regNum = PhysicalReg_EDX;
        infoArray[4].refCount = 2; //for mul_opc
        infoArray[4].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 5;

    case OP_DIV_LONG:
    case OP_REM_LONG:
    case OP_DIV_LONG_2ADDR:
    case OP_REM_LONG_2ADDR:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[0].shareWithVR = false;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 3;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 1;
        infoArray[2].refCount = 2;
        infoArray[2].physicalType = LowOpndRegType_xmm;
        infoArray[3].regNum = PhysicalReg_EAX;
        infoArray[3].refCount = 2; //defined by function call
        infoArray[3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[4].regNum = PhysicalReg_EDX;
        infoArray[4].refCount = 2; //next version has 2 references
        infoArray[4].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[5].regNum = 1;
        infoArray[5].refCount = 2;
        infoArray[5].physicalType = LowOpndRegType_scratch;
        return 6;

    case OP_ADD_FLOAT:
    case OP_SUB_FLOAT:
    case OP_MUL_FLOAT:
    case OP_ADD_FLOAT_2ADDR:
    case OP_SUB_FLOAT_2ADDR:
    case OP_MUL_FLOAT_2ADDR:
    case OP_ADD_DOUBLE: //PhysicalReg_FP TODO
    case OP_SUB_DOUBLE:
    case OP_MUL_DOUBLE:
    case OP_ADD_DOUBLE_2ADDR:
    case OP_SUB_DOUBLE_2ADDR:
    case OP_MUL_DOUBLE_2ADDR:
    case OP_DIV_FLOAT:
    case OP_DIV_FLOAT_2ADDR:
    case OP_DIV_DOUBLE:
    case OP_DIV_DOUBLE_2ADDR:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        //for ALU ops with 2ADDR, the temp variable can share the same physical
        //reg as the virtual register, since the content of VR is updated by
        //the content of the temp variable
        if(inst_op == OP_ADD_FLOAT || inst_op == OP_SUB_FLOAT ||
           inst_op == OP_MUL_FLOAT || inst_op == OP_ADD_DOUBLE ||
           inst_op == OP_SUB_DOUBLE || inst_op == OP_MUL_DOUBLE ||
           inst_op == OP_DIV_FLOAT || inst_op == OP_DIV_DOUBLE)
        infoArray[0].shareWithVR = false;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_xmm;
        return 2;
    case OP_REM_FLOAT:
    case OP_REM_FLOAT_2ADDR:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 1;
        infoArray[2].refCount = 2;
        infoArray[2].physicalType = LowOpndRegType_scratch;
        return 3;

    case OP_REM_DOUBLE:
    case OP_REM_DOUBLE_2ADDR:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_xmm;
        infoArray[2].regNum = 1;
        infoArray[2].refCount = 2;
        infoArray[2].physicalType = LowOpndRegType_scratch;
        return 3;

    case OP_CMPL_FLOAT:
    case OP_CMPL_DOUBLE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = 1;
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 2;
        infoArray[2].refCount = 2;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 3;
        infoArray[3].refCount = 2;
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 4; //return
        infoArray[4].refCount = 5;
        infoArray[4].physicalType = LowOpndRegType_gp;
        return 5;

    case OP_CMPG_FLOAT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = 1;
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 2;
        infoArray[2].refCount = 3;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 3;
        infoArray[3].refCount = 5;
        infoArray[3].physicalType = LowOpndRegType_gp;
        return 4;
        break;
    case OP_CMPG_DOUBLE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2;
        infoArray[0].physicalType = LowOpndRegType_xmm;
        infoArray[1].regNum = 1;
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 2;
        infoArray[2].refCount = 3;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 3;
        infoArray[3].refCount = 5;
        infoArray[3].physicalType = LowOpndRegType_gp;
        return 4;

    case OP_CMP_LONG:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 3;
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 4;
        infoArray[3].refCount = 3;
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 5;
        infoArray[4].refCount = 2;
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[5].regNum = 6;
        infoArray[5].refCount = 7;
        infoArray[5].physicalType = LowOpndRegType_gp;
        return 6;

    case OP_EXECUTE_INLINE:
    case OP_EXECUTE_INLINE_RANGE:
        if(inst_op == OP_EXECUTE_INLINE)
            num = INST_B(inst);
        else
            num = INST_AA(inst);
        tmp = FETCH(1);
        switch (tmp) {
            case INLINE_STRING_LENGTH:
                infoArray[0].regNum = 1;
                infoArray[0].refCount = 3;
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 2;
                infoArray[1].refCount = 2;
                infoArray[1].physicalType = LowOpndRegType_gp;
                infoArray[2].regNum = 3;
                infoArray[2].refCount = 2;
                infoArray[2].physicalType = LowOpndRegType_gp;
                infoArray[3].regNum = 1;
                infoArray[3].refCount = 2;
                infoArray[3].physicalType = LowOpndRegType_scratch;
                return 4;
            case INLINE_STRING_IS_EMPTY:
                infoArray[0].regNum = 1;
                infoArray[0].refCount = 3;
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 2;
                infoArray[1].refCount = 4;
                infoArray[1].physicalType = LowOpndRegType_gp;
                infoArray[2].regNum = 1;
                infoArray[2].refCount = 2;
                infoArray[2].physicalType = LowOpndRegType_scratch;
                return 3;
            case INLINE_STRING_FASTINDEXOF_II:
#if defined(USE_GLOBAL_STRING_DEFS)
                break;
#else
                infoArray[0].regNum = 1;
                infoArray[0].refCount = 14 * LOOP_COUNT;
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 2;
                infoArray[1].refCount = 3 * LOOP_COUNT;
                infoArray[1].physicalType = LowOpndRegType_gp;
                infoArray[2].regNum = 3;
                infoArray[2].refCount = 11 * LOOP_COUNT;
                infoArray[2].physicalType = LowOpndRegType_gp;
                infoArray[3].regNum = 4;
                infoArray[3].refCount = 3 * LOOP_COUNT;
                infoArray[3].physicalType = LowOpndRegType_gp;
                infoArray[4].regNum = 5;
                infoArray[4].refCount = 9 * LOOP_COUNT;
                infoArray[4].physicalType = LowOpndRegType_gp;
                infoArray[5].regNum = 6;
                infoArray[5].refCount = 4 * LOOP_COUNT;
                infoArray[5].physicalType = LowOpndRegType_gp;
                infoArray[6].regNum = 7;
                infoArray[6].refCount = 2;
                infoArray[6].physicalType = LowOpndRegType_gp;
                infoArray[7].regNum = 1;
                infoArray[7].refCount = 2;
                infoArray[7].physicalType = LowOpndRegType_scratch;
                return 8;
#endif
            case INLINE_MATH_ABS_LONG:
                infoArray[0].regNum = 1;
                infoArray[0].refCount = 7;
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 2;
                infoArray[1].refCount = 2;
                infoArray[1].physicalType = LowOpndRegType_gp;
                infoArray[2].regNum = 3;
                infoArray[2].refCount = 3;
                infoArray[2].physicalType = LowOpndRegType_gp;
                infoArray[3].regNum = 4;
                infoArray[3].refCount = 3;
                infoArray[3].physicalType = LowOpndRegType_gp;
                infoArray[4].regNum = 5;
                infoArray[4].refCount = 2;
                infoArray[4].physicalType = LowOpndRegType_gp;
                infoArray[5].regNum = 6;
                infoArray[5].refCount = 5;
                infoArray[5].physicalType = LowOpndRegType_gp;
                return 6;
            case INLINE_MATH_ABS_INT:
                infoArray[0].regNum = 1;
                infoArray[0].refCount = 5;
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 2;
                infoArray[1].refCount = 4;
                infoArray[1].physicalType = LowOpndRegType_gp;
                infoArray[2].regNum = 3;
                infoArray[2].refCount = 2;
                infoArray[2].physicalType = LowOpndRegType_gp;
                return 3;
            case INLINE_MATH_MAX_INT:
                infoArray[0].regNum = 1;
                infoArray[0].refCount = 4;
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 2;
                infoArray[1].refCount = 3;
                infoArray[1].physicalType = LowOpndRegType_gp;
                infoArray[2].regNum = 3;
                infoArray[2].refCount = 2;
                infoArray[2].physicalType = LowOpndRegType_gp;
                return 3;
            case INLINE_MATH_ABS_FLOAT:
                infoArray[0].regNum = 1;
                infoArray[0].refCount = 3;
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 2;
                infoArray[1].refCount = 2;
                infoArray[1].physicalType = LowOpndRegType_gp;
                return 2;
            case INLINE_MATH_ABS_DOUBLE:
                infoArray[0].regNum = 1;
                infoArray[0].refCount = 2;
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 2;
                infoArray[1].refCount = 3;
                infoArray[1].physicalType = LowOpndRegType_gp;
                infoArray[2].regNum = 3;
                infoArray[2].refCount = 3;
                infoArray[2].physicalType = LowOpndRegType_gp;
                return 3;
            case INLINE_FLOAT_TO_RAW_INT_BITS:
                infoArray[0].regNum = 1;
                infoArray[0].refCount = 2;
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 2;
                infoArray[1].refCount = 2;
                infoArray[1].physicalType = LowOpndRegType_gp;
                return 2;
            case INLINE_INT_BITS_TO_FLOAT:
                infoArray[0].regNum = 1;
                infoArray[0].refCount = 2;
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 2;
                infoArray[1].refCount = 2;
                infoArray[1].physicalType = LowOpndRegType_gp;
                return 2;
            case INLINE_DOUBLE_TO_RAW_LONG_BITS:
                infoArray[0].regNum = 1;
                infoArray[0].refCount = 2;
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 2;
                infoArray[1].refCount = 2;
                infoArray[1].physicalType = LowOpndRegType_gp;
                infoArray[2].regNum = 3;
                infoArray[2].refCount = 3;
                infoArray[2].physicalType = LowOpndRegType_gp;
                return 3;
            case INLINE_LONG_BITS_TO_DOUBLE:
                infoArray[0].regNum = 1;
                infoArray[0].refCount = 2;
                infoArray[0].physicalType = LowOpndRegType_gp;
                infoArray[1].regNum = 2;
                infoArray[1].refCount = 2;
                infoArray[1].physicalType = LowOpndRegType_gp;
                infoArray[2].regNum = 3;
                infoArray[2].refCount = 3;
                infoArray[2].physicalType = LowOpndRegType_gp;
                return 3;
            default:
                break;
        }

        infoArray[0].regNum = 1;
        infoArray[0].refCount = 4;
        infoArray[0].physicalType = LowOpndRegType_gp;
        if(num >= 1) {
            infoArray[1].regNum = 2;
            infoArray[1].refCount = 2;
            infoArray[1].physicalType = LowOpndRegType_gp;
        }
        if(num >= 2) {
            infoArray[2].regNum = 3;
            infoArray[2].refCount = 2;
            infoArray[2].physicalType = LowOpndRegType_gp;
        }
        if(num >= 3) {
            infoArray[3].regNum = 4;
            infoArray[3].refCount = 2;
            infoArray[3].physicalType = LowOpndRegType_gp;
        }
        if(num >= 4) {
            infoArray[4].regNum = 5;
            infoArray[4].refCount = 2;
            infoArray[4].physicalType = LowOpndRegType_gp;
        }
        infoArray[num+1].regNum = 6;
        infoArray[num+1].refCount = 2;
        infoArray[num+1].physicalType = LowOpndRegType_gp;
        infoArray[num+2].regNum = PhysicalReg_EAX;
        infoArray[num+2].refCount = 2;
        infoArray[num+2].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[num+3].regNum = PhysicalReg_EDX;
        infoArray[num+3].refCount = 2;
        infoArray[num+3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[num+4].regNum = 1;
        infoArray[num+4].refCount = 4;
        infoArray[num+4].physicalType = LowOpndRegType_scratch;
        return num+5;
#if FIXME
    case OP_INVOKE_OBJECT_INIT_RANGE:
        return 0;
#endif
    case OP_INVOKE_VIRTUAL_QUICK:
    case OP_INVOKE_VIRTUAL_QUICK_RANGE:
#ifdef PREDICTED_CHAINING
        numTmps = updateGenPrediction(infoArray, false /*not interface*/);
        infoArray[numTmps].regNum = 1;
        infoArray[numTmps].refCount = 3; //DU
        infoArray[numTmps].physicalType = LowOpndRegType_gp;
        numTmps++;
        if(inst_op == OP_INVOKE_VIRTUAL_QUICK)
            k = updateInvokeNoRange(infoArray, numTmps);
        else
            k = updateInvokeRange(infoArray, numTmps);
        return k;
#else
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2;
        infoArray[2].physicalType = LowOpndRegType_gp;

        infoArray[3].regNum = PhysicalReg_ECX;
        infoArray[3].refCount = 1+1;
        infoArray[3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[4].regNum = PhysicalReg_EDX;
        infoArray[4].refCount = 2;
        infoArray[4].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        if(inst_op == OP_INVOKE_VIRTUAL_QUICK_RANGE)
            k = updateInvokeRange(infoArray, 5);
        else
            k = updateInvokeNoRange(infoArray, 5);
        return k;
#endif
    case OP_INVOKE_SUPER_QUICK:
    case OP_INVOKE_SUPER_QUICK_RANGE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2;
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 4;
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 5;
        infoArray[2].refCount = 2;
        infoArray[2].physicalType = LowOpndRegType_gp;

        infoArray[3].regNum = PhysicalReg_ECX;
        infoArray[3].refCount = 1+1;
        infoArray[3].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        infoArray[4].regNum = PhysicalReg_EDX;
        infoArray[4].refCount = 2;
        infoArray[4].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;

        infoArray[5].regNum = 1;
        infoArray[5].refCount = 2;
        infoArray[5].physicalType = LowOpndRegType_scratch;
        infoArray[6].regNum = 2;
        infoArray[6].refCount = 2;
        infoArray[6].physicalType = LowOpndRegType_scratch;
        if(inst_op == OP_INVOKE_SUPER_QUICK_RANGE)
            k = updateInvokeRange(infoArray, 7);
        else
            k = updateInvokeNoRange(infoArray, 7);
        return k;
#ifdef SUPPORT_HLO
    case kExtInstruction:
        switch(inst) {
    case OP_X_AGET_QUICK:
    case OP_X_AGET_OBJECT_QUICK:
    case OP_X_AGET_BOOLEAN_QUICK:
    case OP_X_AGET_BYTE_QUICK:
    case OP_X_AGET_CHAR_QUICK:
    case OP_X_AGET_SHORT_QUICK:
        vA = FETCH(1) & 0xff;
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 4;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[3].linkageToVR = vA;
        if(inst == OP_X_AGET_BYTE_QUICK || inst == OP_X_AGET_BOOLEAN_QUICK)
            infoArray[3].is8Bit = true;
        return 4;
    case OP_X_AGET_WIDE_QUICK:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 1;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_xmm;
        return 4;
    case OP_X_APUT_QUICK:
    case OP_X_APUT_OBJECT_QUICK:
    case OP_X_APUT_BOOLEAN_QUICK:
    case OP_X_APUT_BYTE_QUICK:
    case OP_X_APUT_CHAR_QUICK:
    case OP_X_APUT_SHORT_QUICK:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 4;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        if(inst == OP_X_APUT_BYTE_QUICK || inst == OP_X_APUT_BOOLEAN_QUICK)
            infoArray[3].is8Bit = true;
        return 4;
    case OP_X_APUT_WIDE_QUICK:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 1;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_xmm;
        return 4;
    case OP_X_DEREF_GET:
    case OP_X_DEREF_GET_OBJECT:
    case OP_X_DEREF_GET_BOOLEAN:
    case OP_X_DEREF_GET_BYTE:
    case OP_X_DEREF_GET_CHAR:
    case OP_X_DEREF_GET_SHORT:
        vA = FETCH(1) & 0xff;
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[1].linkageToVR = vA;
        if(inst == OP_X_DEREF_GET_BYTE || inst == OP_X_DEREF_GET_BOOLEAN)
            infoArray[1].is8Bit = true;
        return 2;
    case OP_X_DEREF_GET_WIDE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 1;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_xmm;
        return 2;
    case OP_X_DEREF_PUT:
    case OP_X_DEREF_PUT_OBJECT:
    case OP_X_DEREF_PUT_BOOLEAN:
    case OP_X_DEREF_PUT_BYTE:
    case OP_X_DEREF_PUT_CHAR:
    case OP_X_DEREF_PUT_SHORT:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        if(inst == OP_X_DEREF_PUT_BYTE || inst == OP_X_DEREF_PUT_BOOLEAN)
            infoArray[1].is8Bit = true;
        return 2;
    case OP_X_DEREF_PUT_WIDE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 1;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_xmm;
        return 2;
    case OP_X_ARRAY_CHECKS:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        return 2;
    case OP_X_CHECK_BOUNDS:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 2; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        return 2;
    case OP_X_CHECK_NULL:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 2; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = PhysicalReg_EDX;
        infoArray[1].refCount = 2;
        infoArray[1].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 2;
    case OP_X_CHECK_TYPE:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 3; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 5;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 6;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 1;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_scratch;
        infoArray[5].regNum = PhysicalReg_EAX;
        infoArray[5].refCount = 2;
        infoArray[5].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 6;
    case OP_X_ARRAY_OBJECT_CHECKS:
        infoArray[0].regNum = 1;
        infoArray[0].refCount = 3; //DU
        infoArray[0].physicalType = LowOpndRegType_gp;
        infoArray[1].regNum = 2;
        infoArray[1].refCount = 4; //DU
        infoArray[1].physicalType = LowOpndRegType_gp;
        infoArray[2].regNum = 3;
        infoArray[2].refCount = 2; //DU
        infoArray[2].physicalType = LowOpndRegType_gp;
        infoArray[3].regNum = 5;
        infoArray[3].refCount = 2; //DU
        infoArray[3].physicalType = LowOpndRegType_gp;
        infoArray[4].regNum = 6;
        infoArray[4].refCount = 2; //DU
        infoArray[4].physicalType = LowOpndRegType_gp;
        infoArray[5].regNum = 1;
        infoArray[5].refCount = 2; //DU
        infoArray[5].physicalType = LowOpndRegType_scratch;
        infoArray[6].regNum = PhysicalReg_EAX;
        infoArray[6].refCount = 2;
        infoArray[6].physicalType = LowOpndRegType_gp | LowOpndRegType_hard;
        return 7;
    }
#endif
    }
    return -1;
}
