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


/*! \file Lower.cpp
    \brief This file implements the high-level wrapper for lowering

*/

//#include "uthash.h"
#include "libdex/DexOpcodes.h"
#include "libdex/DexFile.h"
#include <math.h>
#include <sys/mman.h>
#include "Translator.h"
#include "Lower.h"
#include "enc_wrapper.h"
#include "vm/mterp/Mterp.h"
#include "NcgHelper.h"
#include "libdex/DexCatch.h"
#include "compiler/CompilerIR.h"

//statistics for optimization
int num_removed_nullCheck;

PhysicalReg scratchRegs[4];

LowOp* ops[BUFFER_SIZE];
LowOp* op;
u2* rPC; //PC pointer to bytecode
u2 inst; //current bytecode
int offsetPC/*offset in bytecode*/, offsetNCG/*byte offset in native code*/;
int ncg_rPC;
//! map from PC in bytecode to PC in native code
int mapFromBCtoNCG[BYTECODE_SIZE_PER_METHOD]; //initially mapped to -1
char* streamStart = NULL; //start of the Pure CodeItem?, not include the global symbols
char* streamCode = NULL; //start of the Pure CodeItem?, not include the global symbols
char* streamMethodStart; //start of the method
char* stream; //current stream pointer
int lowOpTimeStamp = 0;
Method* currentMethod = NULL;
int currentExceptionBlockIdx = -1;
LowOpBlockLabel* traceLabelList = NULL;
BasicBlock* traceCurrentBB = NULL;
MIR* traceCurrentMIR = NULL;
bool scheduling_is_on = false;

int common_invokeMethodNoRange();
int common_invokeMethodRange();
int common_invokeArgsDone(ArgsDoneType, bool);

//data section of .ia32:
char globalData[128];

char strClassCastException[] = "Ljava/lang/ClassCastException;";
char strInstantiationError[] = "Ljava/lang/InstantiationError;";
char strInternalError[] = "Ljava/lang/InternalError;";
char strFilledNewArrayNotImpl[] = "filled-new-array only implemented for 'int'";
char strArithmeticException[] = "Ljava/lang/ArithmeticException;";
char strArrayIndexException[] = "Ljava/lang/ArrayIndexOutOfBoundsException;";
char strArrayStoreException[] = "Ljava/lang/ArrayStoreException;";
char strDivideByZero[] = "divide by zero";
char strNegativeArraySizeException[] = "Ljava/lang/NegativeArraySizeException;";
char strNoSuchMethodError[] = "Ljava/lang/NoSuchMethodError;";
char strNullPointerException[] = "Ljava/lang/NullPointerException;";
char strStringIndexOutOfBoundsException[] = "Ljava/lang/StringIndexOutOfBoundsException;";

int LstrClassCastExceptionPtr, LstrInstantiationErrorPtr, LstrInternalError, LstrFilledNewArrayNotImpl;
int LstrArithmeticException, LstrArrayIndexException, LstrArrayStoreException, LstrStringIndexOutOfBoundsException;
int LstrDivideByZero, LstrNegativeArraySizeException, LstrNoSuchMethodError, LstrNullPointerException;
int LdoubNeg, LvaluePosInfLong, LvalueNegInfLong, LvalueNanLong, LshiftMask, Lvalue64, L64bits, LintMax, LintMin;

void initConstDataSec() {
    char* tmpPtr = globalData;

    LdoubNeg = (int)tmpPtr;
    *((u4*)tmpPtr) = 0x00000000;
    tmpPtr += sizeof(u4);
    *((u4*)tmpPtr) = 0x80000000;
    tmpPtr += sizeof(u4);

    LvaluePosInfLong = (int)tmpPtr;
    *((u4*)tmpPtr) = 0xFFFFFFFF;
    tmpPtr += sizeof(u4);
    *((u4*)tmpPtr) = 0x7FFFFFFF;
    tmpPtr += sizeof(u4);

    LvalueNegInfLong = (int)tmpPtr;
    *((u4*)tmpPtr) = 0x00000000;
    tmpPtr += sizeof(u4);
    *((u4*)tmpPtr) = 0x80000000;
    tmpPtr += sizeof(u4);

    LvalueNanLong = (int)tmpPtr;
    *((u4*)tmpPtr) = 0;
    tmpPtr += sizeof(u4);
    *((u4*)tmpPtr) = 0;
    tmpPtr += sizeof(u4);

    LshiftMask = (int)tmpPtr;
    *((u4*)tmpPtr) = 0x3f;
    tmpPtr += sizeof(u4);
    *((u4*)tmpPtr) = 0;
    tmpPtr += sizeof(u4);

    Lvalue64 = (int)tmpPtr;
    *((u4*)tmpPtr) = 0x40;
    tmpPtr += sizeof(u4);
    *((u4*)tmpPtr) = 0;
    tmpPtr += sizeof(u4);

    L64bits = (int)tmpPtr;
    *((u4*)tmpPtr) = 0xFFFFFFFF;
    tmpPtr += sizeof(u4);
    *((u4*)tmpPtr) = 0xFFFFFFFF;
    tmpPtr += sizeof(u4);

    LintMin = (int)tmpPtr;
    *((u4*)tmpPtr) = 0x80000000;
    tmpPtr += sizeof(u4);

    LintMax = (int)tmpPtr;
    *((u4*)tmpPtr) = 0x7FFFFFFF;
    tmpPtr += sizeof(u4);

    LstrClassCastExceptionPtr = (int)strClassCastException;
    LstrInstantiationErrorPtr = (int)strInstantiationError;
    LstrInternalError = (int)strInternalError;
    LstrFilledNewArrayNotImpl = (int)strFilledNewArrayNotImpl;
    LstrArithmeticException = (int)strArithmeticException;
    LstrArrayIndexException = (int)strArrayIndexException;
    LstrArrayStoreException = (int)strArrayStoreException;
    LstrDivideByZero = (int)strDivideByZero;
    LstrNegativeArraySizeException = (int)strNegativeArraySizeException;
    LstrNoSuchMethodError = (int)strNoSuchMethodError;
    LstrNullPointerException = (int)strNullPointerException;
    LstrStringIndexOutOfBoundsException = (int)strStringIndexOutOfBoundsException;
}

//declarations of functions used in this file
int spill_reg(int reg, bool isPhysical);
int unspill_reg(int reg, bool isPhysical);

int const_string_resolve();
int sget_sput_resolve();
int new_instance_needinit();
int new_instance_abstract();
int invoke_virtual_resolve();
int invoke_direct_resolve();
int invoke_static_resolve();
int filled_new_array_notimpl();
int resolve_class2(
                   int startLR/*logical register index*/, bool isPhysical, int indexReg/*const pool index*/,
                   bool indexPhysical,
                   int thirdArg);
int resolve_method2(
                    int startLR/*logical register index*/, bool isPhysical, int indexReg/*const pool index*/,
                    bool indexPhysical,
                    int thirdArg/*VIRTUAL*/);
int resolve_inst_field2(
                        int startLR/*logical register index*/, bool isPhysical,
                        int indexReg/*const pool index*/,
                        bool indexPhysical);
int resolve_static_field2(
                          int startLR/*logical register index*/, bool isPhysical,
                          int indexReg/*const pool index*/,
                          bool indexPhysical);

int invokeMethodNoRange_1_helper();
int invokeMethodNoRange_2_helper();
int invokeMethodNoRange_3_helper();
int invokeMethodNoRange_4_helper();
int invokeMethodNoRange_5_helper();
int invokeMethodRange_helper();

int invoke_virtual_helper();
int invoke_virtual_quick_helper();
int invoke_static_helper();
int invoke_direct_helper();
int new_instance_helper();
int sget_sput_helper(int flag);
int aput_obj_helper();
int aget_helper(int flag);
int aput_helper(int flag);
int monitor_enter_helper();
int monitor_exit_helper();
int throw_helper();
int const_string_helper();
int array_length_helper();
int invoke_super_helper();
int invoke_interface_helper();
int iget_iput_helper(int flag);
int check_cast_helper(bool instance);
int new_array_helper();

int common_returnFromMethod();

/*!
\brief dump helper functions

*/
int performCGWorklist() {
    filled_new_array_notimpl();
    freeShortMap();
    const_string_resolve();
    freeShortMap();

    resolve_class2(PhysicalReg_EAX, true, PhysicalReg_EAX, true, 0);
    freeShortMap();
    resolve_method2(PhysicalReg_EAX, true, PhysicalReg_EAX, true, METHOD_VIRTUAL);
    freeShortMap();
    resolve_method2(PhysicalReg_EAX, true, PhysicalReg_EAX, true, METHOD_DIRECT);
    freeShortMap();
    resolve_method2(PhysicalReg_EAX, true, PhysicalReg_EAX, true, METHOD_STATIC);
    freeShortMap();
    resolve_inst_field2(PhysicalReg_EAX, true, PhysicalReg_EAX, true);
    freeShortMap();
    resolve_static_field2(PhysicalReg_EAX, true, PhysicalReg_EAX, true);
    freeShortMap();
    throw_exception_message(PhysicalReg_ECX, PhysicalReg_EAX, true, PhysicalReg_Null, true);
    freeShortMap();
    throw_exception(PhysicalReg_ECX, PhysicalReg_EAX, PhysicalReg_Null, true);
    freeShortMap();
    new_instance_needinit();
    freeShortMap();
    return 0;
}

int aput_object_count;
int common_periodicChecks_entry();
int common_periodicChecks4();
/*!
\brief for debugging purpose, dump the sequence of native code for each bytecode

*/
int ncgMethodFake(Method* method) {
    //to measure code size expansion, no need to patch up labels
    methodDataWorklist = NULL;
    globalShortWorklist = NULL;
    globalNCGWorklist = NULL;
    streamMethodStart = stream;

    //initialize mapFromBCtoNCG
    memset(&mapFromBCtoNCG[0], -1, BYTECODE_SIZE_PER_METHOD * sizeof(mapFromBCtoNCG[0]));
    unsigned int i;
    u2* rStart = (u2*)malloc(5*sizeof(u2));
    if(rStart == NULL) {
        ALOGE("Memory allocation failed");
        return -1;
    }
    rPC = rStart;
    method->insns = rStart;
    for(i = 0; i < 5; i++) *rPC++ = 0;
    for(i = 0; i < 256; i++) {
        rPC = rStart;
        //modify the opcode
        char* tmp = (char*)rStart;
        *tmp++ = i;
        *tmp = i;
        inst = FETCH(0);
        char* tmpStart = stream;
        lowerByteCode(method); //use inst, rPC, method, modify rPC
        int size_in_u2 = rPC - rStart;
        if(stream - tmpStart  > 0)
            ALOGI("LOWER bytecode %x size in u2: %d ncg size in byte: %d", i, size_in_u2, stream - tmpStart);
    }
    exit(0);
}

bool existATryBlock(Method* method, int startPC, int endPC) {
    const DexCode* pCode = dvmGetMethodCode(method);
    u4 triesSize = pCode->triesSize;
    const DexTry* pTries = dexGetTries(pCode);
    unsigned int i;
    for (i = 0; i < triesSize; i++) {
        const DexTry* pTry = &pTries[i];
        u4 start = pTry->startAddr; //offsetPC
        u4 end = start + pTry->insnCount;
        //if [start, end] overlaps with [startPC, endPC] returns true
        if((int)end < startPC || (int)start > endPC) { //no overlap
        } else {
            return true;
        }
    }
    return false;
}

int mm_bytecode_size = 0;
int mm_ncg_size = 0;
int mm_relocation_size = 0;
int mm_map_size = 0;
void resetCodeSize() {
    mm_bytecode_size = 0;
    mm_ncg_size = 0;
    mm_relocation_size = 0;
    mm_map_size = 0;
}

bool bytecodeIsRemoved(const Method* method, u4 bytecodeOffset) {
    if(gDvm.executionMode == kExecutionModeNcgO0) return false;
    u4 ncgOff = mapFromBCtoNCG[bytecodeOffset];
    int k = bytecodeOffset+1;
    u2 insnsSize = dvmGetMethodInsnsSize(method);
    while(k < insnsSize) {
        if(mapFromBCtoNCG[k] < 0) {
            k++;
            continue;
        }
        if(mapFromBCtoNCG[k] == (int)ncgOff) return true;
        return false;
    }
    return false;
}

int invoke_super_nsm();
void init_common(const char* curFileName, DvmDex *pDvmDex, bool forNCG); //forward declaration
void initGlobalMethods(); //forward declaration

//called once when compiler thread starts up
void initJIT(const char* curFileName, DvmDex *pDvmDex) {
    init_common(curFileName, pDvmDex, false);
}

void init_common(const char* curFileName, DvmDex *pDvmDex, bool forNCG) {
    if(!gDvm.constInit) {
        globalMapNum = 0;
        globalMap = NULL;
        initConstDataSec();
        gDvm.constInit = true;
    }

    //for initJIT: stream is already set
    if(!gDvm.commonInit) {
        initGlobalMethods();
        gDvm.commonInit = true;
    }
}

void initGlobalMethods() {
    dump_x86_inst = false; /* DEBUG */
    // generate native code for function ncgGetEIP
    insertLabel("ncgGetEIP", false);
    move_mem_to_reg(OpndSize_32, 0, PhysicalReg_ESP, true, PhysicalReg_EDX, true);
    x86_return();

    //generate code for common labels
    //jumps within a helper function is treated as short labels
    globalShortMap = NULL;
    common_periodicChecks_entry();
    freeShortMap();
    common_periodicChecks4();
    freeShortMap();
    //common_invokeMethodNoRange();
    //common_invokeMethodRange();

    if(dump_x86_inst) ALOGI("ArgsDone_Normal start");
    common_invokeArgsDone(ArgsDone_Normal, false);
    freeShortMap();
    if(dump_x86_inst) ALOGI("ArgsDone_Native start");
    common_invokeArgsDone(ArgsDone_Native, false);
    freeShortMap();
    if(dump_x86_inst) ALOGI("ArgsDone_Full start");
    common_invokeArgsDone(ArgsDone_Full, true/*isJitFull*/);
    if(dump_x86_inst) ALOGI("ArgsDone_Full end");
    freeShortMap();

    common_backwardBranch();
    freeShortMap();
    common_exceptionThrown();
    freeShortMap();
    common_errNullObject();
    freeShortMap();
    common_errArrayIndex();
    freeShortMap();
    common_errArrayStore();
    freeShortMap();
    common_errNegArraySize();
    freeShortMap();
    common_errNoSuchMethod();
    freeShortMap();
    common_errDivideByZero();
    freeShortMap();
    common_gotoBail();
    freeShortMap();
    common_gotoBail_0();
    freeShortMap();
    invoke_super_nsm();
    freeShortMap();

    performCGWorklist(); //generate code for helper functions
    performLabelWorklist(); //it is likely that the common labels will jump to other common labels

    dump_x86_inst = false;
}

ExecutionMode origMode;
//when to update streamMethodStart
bool lowerByteCodeJit(const Method* method, const u2* codePtr, MIR* mir) {
    rPC = (u2*)codePtr;
    inst = FETCH(0);
    traceCurrentMIR = mir;
    int retCode = lowerByteCode(method);
    traceCurrentMIR = NULL;
    freeShortMap();
    if(retCode >= 0) return false; //handled
    return true; //not handled
}

void startOfBasicBlock(BasicBlock* bb) {
    traceCurrentBB = bb;
    if(gDvm.executionMode == kExecutionModeNcgO0) {
        isScratchPhysical = true;
    } else {
        isScratchPhysical = false;
    }
}

void startOfTrace(const Method* method, LowOpBlockLabel* labelList, int exceptionBlockId,
                  CompilationUnit *cUnit) {
    origMode = gDvm.executionMode;
    gDvm.executionMode = kExecutionModeNcgO1;
    if(gDvm.executionMode == kExecutionModeNcgO0) {
        isScratchPhysical = true;
    } else {
        isScratchPhysical = false;
    }
    currentMethod = (Method*)method;
    currentExceptionBlockIdx = exceptionBlockId;
    methodDataWorklist = NULL;
    globalShortWorklist = NULL;
    globalNCGWorklist = NULL;

    streamMethodStart = stream;
    //initialize mapFromBCtoNCG
    memset(&mapFromBCtoNCG[0], -1, BYTECODE_SIZE_PER_METHOD * sizeof(mapFromBCtoNCG[0]));
    traceLabelList = labelList;
    if(gDvm.executionMode == kExecutionModeNcgO1)
        startOfTraceO1(method, labelList, exceptionBlockId, cUnit);
}

void endOfTrace(bool freeOnly) {
    if(freeOnly) {
        freeLabelWorklist();
        freeNCGWorklist();
        freeDataWorklist();
        freeChainingWorklist();
    }
    else {
        performLabelWorklist();
        performNCGWorklist(); //handle forward jump (GOTO, IF)
        performDataWorklist(); //handle SWITCH & FILL_ARRAY_DATA
        performChainingWorklist();
    }
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        endOfTraceO1();
    }
    gDvm.executionMode = origMode;
}

///////////////////////////////////////////////////////////////////
//!
//! each bytecode is translated to a sequence of machine codes
int lowerByteCode(const Method* method) { //inputs: rPC & inst & stream & streamMethodStart
    /* offsetPC is used in O1 code generator, where it is defined as the sequence number
       use a local version to avoid overwriting */
    int offsetPC = rPC - (u2*)method->insns;

    if(dump_x86_inst)
        ALOGI("LOWER bytecode %x at offsetPC %x offsetNCG %x @%p",
              INST_INST(inst), offsetPC, stream - streamMethodStart, stream);

    //update mapFromBCtoNCG
    offsetNCG = stream - streamMethodStart;
    if(offsetPC >= BYTECODE_SIZE_PER_METHOD) ALOGE("offsetPC %d exceeds BYTECODE_SIZE_PER_METHOD", offsetPC);
    mapFromBCtoNCG[offsetPC] = offsetNCG;
#if defined(ENABLE_TRACING) && defined(TRACING_OPTION2)
    insertMapWorklist(offsetPC, mapFromBCtoNCG[offsetPC], 1);
#endif
    //return number of LowOps generated
    switch (INST_INST(inst)) {
    case OP_NOP:
        return op_nop();
    case OP_MOVE:
    case OP_MOVE_OBJECT:
        return op_move();
    case OP_MOVE_FROM16:
    case OP_MOVE_OBJECT_FROM16:
        return op_move_from16();
    case OP_MOVE_16:
    case OP_MOVE_OBJECT_16:
        return op_move_16();
    case OP_MOVE_WIDE:
        return op_move_wide();
    case OP_MOVE_WIDE_FROM16:
        return op_move_wide_from16();
    case OP_MOVE_WIDE_16:
        return op_move_wide_16();
    case OP_MOVE_RESULT:
    case OP_MOVE_RESULT_OBJECT:
        return op_move_result();
    case OP_MOVE_RESULT_WIDE:
        return op_move_result_wide();
    case OP_MOVE_EXCEPTION:
        return op_move_exception();
    case OP_RETURN_VOID:
    case OP_RETURN_VOID_BARRIER:
        return op_return_void();
    case OP_RETURN:
    case OP_RETURN_OBJECT:
        return op_return();
    case OP_RETURN_WIDE:
        return op_return_wide();
    case OP_CONST_4:
        return op_const_4();
    case OP_CONST_16:
        return op_const_16();
    case OP_CONST:
        return op_const();
    case OP_CONST_HIGH16:
        return op_const_high16();
    case OP_CONST_WIDE_16:
        return op_const_wide_16();
    case OP_CONST_WIDE_32:
        return op_const_wide_32();
    case OP_CONST_WIDE:
        return op_const_wide();
    case OP_CONST_WIDE_HIGH16:
        return op_const_wide_high16();
    case OP_CONST_STRING:
        return op_const_string();
    case OP_CONST_STRING_JUMBO:
        return op_const_string_jumbo();
    case OP_CONST_CLASS:
        return op_const_class();
    case OP_MONITOR_ENTER:
        return op_monitor_enter();
    case OP_MONITOR_EXIT:
        return op_monitor_exit();
    case OP_CHECK_CAST:
        return op_check_cast();
    case OP_INSTANCE_OF:
        return op_instance_of();
    case OP_ARRAY_LENGTH:
        return op_array_length();
    case OP_NEW_INSTANCE:
        return op_new_instance();
    case OP_NEW_ARRAY:
        return op_new_array();
    case OP_FILLED_NEW_ARRAY:
        return op_filled_new_array();
    case OP_FILLED_NEW_ARRAY_RANGE:
        return op_filled_new_array_range();
    case OP_FILL_ARRAY_DATA:
        return op_fill_array_data();
    case OP_THROW:
        return op_throw();
    case OP_THROW_VERIFICATION_ERROR:
        return op_throw_verification_error();
    case OP_GOTO:
        return op_goto();
    case OP_GOTO_16:
        return op_goto_16();
    case OP_GOTO_32:
        return op_goto_32();
    case OP_PACKED_SWITCH:
        return op_packed_switch();
    case OP_SPARSE_SWITCH:
        return op_sparse_switch();
    case OP_CMPL_FLOAT:
        return op_cmpl_float();
    case OP_CMPG_FLOAT:
        return op_cmpg_float();
    case OP_CMPL_DOUBLE:
        return op_cmpl_double();
    case OP_CMPG_DOUBLE:
        return op_cmpg_double();
    case OP_CMP_LONG:
        return op_cmp_long();
    case OP_IF_EQ:
        return op_if_eq();
    case OP_IF_NE:
        return op_if_ne();
    case OP_IF_LT:
        return op_if_lt();
    case OP_IF_GE:
        return op_if_ge();
    case OP_IF_GT:
        return op_if_gt();
    case OP_IF_LE:
        return op_if_le();
    case OP_IF_EQZ:
        return op_if_eqz();
    case OP_IF_NEZ:
        return op_if_nez();
    case OP_IF_LTZ:
        return op_if_ltz();
    case OP_IF_GEZ:
        return op_if_gez();
    case OP_IF_GTZ:
        return op_if_gtz();
    case OP_IF_LEZ:
        return op_if_lez();
    case OP_AGET:
        return op_aget();
    case OP_AGET_WIDE:
        return op_aget_wide();
    case OP_AGET_OBJECT:
        return op_aget_object();
    case OP_AGET_BOOLEAN:
        return op_aget_boolean();
    case OP_AGET_BYTE:
        return op_aget_byte();
    case OP_AGET_CHAR:
        return op_aget_char();
    case OP_AGET_SHORT:
        return op_aget_short();
    case OP_APUT:
        return op_aput();
    case OP_APUT_WIDE:
        return op_aput_wide();
    case OP_APUT_OBJECT:
        return op_aput_object();
    case OP_APUT_BOOLEAN:
        return op_aput_boolean();
    case OP_APUT_BYTE:
        return op_aput_byte();
    case OP_APUT_CHAR:
        return op_aput_char();
    case OP_APUT_SHORT:
        return op_aput_short();
    case OP_IGET:
    case OP_IGET_VOLATILE:
        return op_iget();
    case OP_IGET_WIDE:
        return op_iget_wide(false); // isVolatile==false
    case OP_IGET_WIDE_VOLATILE:
        return op_iget_wide(true);  // isVolatile==true
    case OP_IGET_OBJECT:
    case OP_IGET_OBJECT_VOLATILE:
        return op_iget_object();
    case OP_IGET_BOOLEAN:
        return op_iget_boolean();
    case OP_IGET_BYTE:
        return op_iget_byte();
    case OP_IGET_CHAR:
        return op_iget_char();
    case OP_IGET_SHORT:
        return op_iget_short();
    case OP_IPUT:
    case OP_IPUT_VOLATILE:
        return op_iput();
    case OP_IPUT_WIDE:
        return op_iput_wide(false); // isVolatile==false
    case OP_IPUT_WIDE_VOLATILE:
        return op_iput_wide(true);  // isVolatile==true
    case OP_IPUT_OBJECT:
    case OP_IPUT_OBJECT_VOLATILE:
        return op_iput_object();
    case OP_IPUT_BOOLEAN:
        return op_iput_boolean();
    case OP_IPUT_BYTE:
        return op_iput_byte();
    case OP_IPUT_CHAR:
        return op_iput_char();
    case OP_IPUT_SHORT:
        return op_iput_short();
    case OP_SGET:
    case OP_SGET_VOLATILE:
        return op_sget();
    case OP_SGET_WIDE:
        return op_sget_wide(false); // isVolatile==false
    case OP_SGET_WIDE_VOLATILE:
        return op_sget_wide(true);  // isVolatile==true
    case OP_SGET_OBJECT:
    case OP_SGET_OBJECT_VOLATILE:
        return op_sget_object();
    case OP_SGET_BOOLEAN:
        return op_sget_boolean();
    case OP_SGET_BYTE:
        return op_sget_byte();
    case OP_SGET_CHAR:
        return op_sget_char();
    case OP_SGET_SHORT:
        return op_sget_short();
    case OP_SPUT:
    case OP_SPUT_VOLATILE:
        return op_sput(false);
    case OP_SPUT_WIDE:
        return op_sput_wide(false); // isVolatile==false
    case OP_SPUT_WIDE_VOLATILE:
        return op_sput_wide(true);  // isVolatile==true
    case OP_SPUT_OBJECT:
    case OP_SPUT_OBJECT_VOLATILE:
        return op_sput_object();
    case OP_SPUT_BOOLEAN:
        return op_sput_boolean();
    case OP_SPUT_BYTE:
        return op_sput_byte();
    case OP_SPUT_CHAR:
        return op_sput_char();
    case OP_SPUT_SHORT:
        return op_sput_short();
    case OP_INVOKE_VIRTUAL:
        return op_invoke_virtual();
    case OP_INVOKE_SUPER:
        return op_invoke_super();
    case OP_INVOKE_DIRECT:
        return op_invoke_direct();
    case OP_INVOKE_STATIC:
        return op_invoke_static();
    case OP_INVOKE_INTERFACE:
        return op_invoke_interface();
    case OP_INVOKE_VIRTUAL_RANGE:
        return op_invoke_virtual_range();
    case OP_INVOKE_SUPER_RANGE:
        return op_invoke_super_range();
    case OP_INVOKE_DIRECT_RANGE:
        return op_invoke_direct_range();
    case OP_INVOKE_STATIC_RANGE:
        return op_invoke_static_range();
    case OP_INVOKE_INTERFACE_RANGE:
        return op_invoke_interface_range();
    case OP_NEG_INT:
        return op_neg_int();
    case OP_NOT_INT:
        return op_not_int();
    case OP_NEG_LONG:
        return op_neg_long();
    case OP_NOT_LONG:
        return op_not_long();
    case OP_NEG_FLOAT:
        return op_neg_float();
    case OP_NEG_DOUBLE:
        return op_neg_double();
    case OP_INT_TO_LONG:
        return op_int_to_long();
    case OP_INT_TO_FLOAT:
        return op_int_to_float();
    case OP_INT_TO_DOUBLE:
        return op_int_to_double();
    case OP_LONG_TO_INT:
        return op_long_to_int();
    case OP_LONG_TO_FLOAT:
        return op_long_to_float();
    case OP_LONG_TO_DOUBLE:
        return op_long_to_double();
    case OP_FLOAT_TO_INT:
        return op_float_to_int();
    case OP_FLOAT_TO_LONG:
        return op_float_to_long();
    case OP_FLOAT_TO_DOUBLE:
        return op_float_to_double();
    case OP_DOUBLE_TO_INT:
        return op_double_to_int();
    case OP_DOUBLE_TO_LONG:
        return op_double_to_long();
    case OP_DOUBLE_TO_FLOAT:
        return op_double_to_float();
    case OP_INT_TO_BYTE:
        return op_int_to_byte();
    case OP_INT_TO_CHAR:
        return op_int_to_char();
    case OP_INT_TO_SHORT:
        return op_int_to_short();
    case OP_ADD_INT:
        return op_add_int();
    case OP_SUB_INT:
        return op_sub_int();
    case OP_MUL_INT:
        return op_mul_int();
    case OP_DIV_INT:
        return op_div_int();
    case OP_REM_INT:
        return op_rem_int();
    case OP_AND_INT:
        return op_and_int();
    case OP_OR_INT:
        return op_or_int();
    case OP_XOR_INT:
        return op_xor_int();
    case OP_SHL_INT:
        return op_shl_int();
    case OP_SHR_INT:
        return op_shr_int();
    case OP_USHR_INT:
        return op_ushr_int();
    case OP_ADD_LONG:
        return op_add_long();
    case OP_SUB_LONG:
        return op_sub_long();
    case OP_MUL_LONG:
        return op_mul_long();
    case OP_DIV_LONG:
        return op_div_long();
    case OP_REM_LONG:
        return op_rem_long();
    case OP_AND_LONG:
        return op_and_long();
    case OP_OR_LONG:
        return op_or_long();
    case OP_XOR_LONG:
        return op_xor_long();
    case OP_SHL_LONG:
        return op_shl_long();
    case OP_SHR_LONG:
        return op_shr_long();
    case OP_USHR_LONG:
        return op_ushr_long();
    case OP_ADD_FLOAT:
        return op_add_float();
    case OP_SUB_FLOAT:
        return op_sub_float();
    case OP_MUL_FLOAT:
        return op_mul_float();
    case OP_DIV_FLOAT:
        return op_div_float();
    case OP_REM_FLOAT:
        return op_rem_float();
    case OP_ADD_DOUBLE:
        return op_add_double();
    case OP_SUB_DOUBLE:
        return op_sub_double();
    case OP_MUL_DOUBLE:
        return op_mul_double();
    case OP_DIV_DOUBLE:
        return op_div_double();
    case OP_REM_DOUBLE:
        return op_rem_double();
    case OP_ADD_INT_2ADDR:
        return op_add_int_2addr();
    case OP_SUB_INT_2ADDR:
        return op_sub_int_2addr();
    case OP_MUL_INT_2ADDR:
        return op_mul_int_2addr();
    case OP_DIV_INT_2ADDR:
        return op_div_int_2addr();
    case OP_REM_INT_2ADDR:
        return op_rem_int_2addr();
    case OP_AND_INT_2ADDR:
        return op_and_int_2addr();
    case OP_OR_INT_2ADDR:
        return op_or_int_2addr();
    case OP_XOR_INT_2ADDR:
        return op_xor_int_2addr();
    case OP_SHL_INT_2ADDR:
        return op_shl_int_2addr();
    case OP_SHR_INT_2ADDR:
        return op_shr_int_2addr();
    case OP_USHR_INT_2ADDR:
        return op_ushr_int_2addr();
    case OP_ADD_LONG_2ADDR:
        return op_add_long_2addr();
    case OP_SUB_LONG_2ADDR:
        return op_sub_long_2addr();
    case OP_MUL_LONG_2ADDR:
        return op_mul_long_2addr();
    case OP_DIV_LONG_2ADDR:
        return op_div_long_2addr();
    case OP_REM_LONG_2ADDR:
        return op_rem_long_2addr();
    case OP_AND_LONG_2ADDR:
        return op_and_long_2addr();
    case OP_OR_LONG_2ADDR:
        return op_or_long_2addr();
    case OP_XOR_LONG_2ADDR:
        return op_xor_long_2addr();
    case OP_SHL_LONG_2ADDR:
        return op_shl_long_2addr();
    case OP_SHR_LONG_2ADDR:
        return op_shr_long_2addr();
    case OP_USHR_LONG_2ADDR:
        return op_ushr_long_2addr();
    case OP_ADD_FLOAT_2ADDR:
        return op_add_float_2addr();
    case OP_SUB_FLOAT_2ADDR:
        return op_sub_float_2addr();
    case OP_MUL_FLOAT_2ADDR:
        return op_mul_float_2addr();
    case OP_DIV_FLOAT_2ADDR:
        return op_div_float_2addr();
    case OP_REM_FLOAT_2ADDR:
        return op_rem_float_2addr();
    case OP_ADD_DOUBLE_2ADDR:
        return op_add_double_2addr();
    case OP_SUB_DOUBLE_2ADDR:
        return op_sub_double_2addr();
    case OP_MUL_DOUBLE_2ADDR:
        return op_mul_double_2addr();
    case OP_DIV_DOUBLE_2ADDR:
        return op_div_double_2addr();
    case OP_REM_DOUBLE_2ADDR:
        return op_rem_double_2addr();
    case OP_ADD_INT_LIT16:
        return op_add_int_lit16();
    case OP_RSUB_INT:
        return op_rsub_int();
    case OP_MUL_INT_LIT16:
        return op_mul_int_lit16();
    case OP_DIV_INT_LIT16:
        return op_div_int_lit16();
    case OP_REM_INT_LIT16:
        return op_rem_int_lit16();
    case OP_AND_INT_LIT16:
        return op_and_int_lit16();
    case OP_OR_INT_LIT16:
        return op_or_int_lit16();
    case OP_XOR_INT_LIT16:
        return op_xor_int_lit16();
    case OP_ADD_INT_LIT8:
        return op_add_int_lit8();
    case OP_RSUB_INT_LIT8:
        return op_rsub_int_lit8();
    case OP_MUL_INT_LIT8:
        return op_mul_int_lit8();
    case OP_DIV_INT_LIT8:
        return op_div_int_lit8();
    case OP_REM_INT_LIT8:
        return op_rem_int_lit8();
    case OP_AND_INT_LIT8:
        return op_and_int_lit8();
    case OP_OR_INT_LIT8:
        return op_or_int_lit8();
    case OP_XOR_INT_LIT8:
        return op_xor_int_lit8();
    case OP_SHL_INT_LIT8:
        return op_shl_int_lit8();
    case OP_SHR_INT_LIT8:
        return op_shr_int_lit8();
    case OP_USHR_INT_LIT8:
        return op_ushr_int_lit8();
    case OP_EXECUTE_INLINE:
        return op_execute_inline(false);
    case OP_EXECUTE_INLINE_RANGE:
        return op_execute_inline(true);
    case OP_BREAKPOINT:
        ALOGE("found bytecode OP_BREAKPOINT");
        dvmAbort();
    case OP_INVOKE_OBJECT_INIT_RANGE:
        return op_invoke_object_init_range();
    case OP_IGET_QUICK:
        return op_iget_quick();
    case OP_IGET_WIDE_QUICK:
        return op_iget_wide_quick();
    case OP_IGET_OBJECT_QUICK:
        return op_iget_object_quick();
    case OP_IPUT_QUICK:
        return op_iput_quick();
    case OP_IPUT_WIDE_QUICK:
        return op_iput_wide_quick();
    case OP_IPUT_OBJECT_QUICK:
        return op_iput_object_quick();
    case OP_INVOKE_VIRTUAL_QUICK:
        return op_invoke_virtual_quick();
    case OP_INVOKE_VIRTUAL_QUICK_RANGE:
        return op_invoke_virtual_quick_range();
    case OP_INVOKE_SUPER_QUICK:
        return op_invoke_super_quick();
    case OP_INVOKE_SUPER_QUICK_RANGE:
        return op_invoke_super_quick_range();
    }

    ALOGE("No JIT support for bytecode %x at offsetPC %x",
          INST_INST(inst), offsetPC);
    return -1;
}
int op_nop() {
    rPC++;
    return 0;
}
