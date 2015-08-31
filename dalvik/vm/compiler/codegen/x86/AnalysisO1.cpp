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


/*! \file AnalysisO1.cpp
  \brief This file implements register allocator, constant folding
*/
#include "libdex/DexOpcodes.h"
#include "libdex/DexFile.h"
#include "Lower.h"
#include "interp/InterpState.h"
#include "interp/InterpDefs.h"
#include "libdex/Leb128.h"

/* compilation flags to turn on debug printout */
//#define DEBUG_COMPILE_TABLE
//#define DEBUG_ALLOC_CONSTRAINT
//#define DEBUG_REGALLOC
//#define DEBUG_REG_USED
//#define DEBUG_REFCOUNT
//#define DEBUG_REACHING_DEF2
//#define DEBUG_REACHING_DEF
//#define DEBUG_LIVE_RANGE
//#define DEBUG_MOVE_OPT
//#define DEBUG_SPILL
//#define DEBUG_ENDOFBB
//#define DEBUG_CONST
/*
  #define DEBUG_XFER_POINTS
  #define DEBUG_DSE
  #define DEBUG_CFG
  #define DEBUG_GLOBALTYPE
  #define DEBUG_STATE
  #define DEBUG_COMPILE_TABLE
  #define DEBUG_VIRTUAL_INFO
  #define DEBUG_MOVE_OPT
  #define DEBUG_MERGE_ENTRY
  #define DEBUG_INVALIDATE
*/
#include "AnalysisO1.h"

void dumpCompileTable();

/* There are 3 kinds of variables that are handled in this file:
   1> virtual register (isVirtualReg())
   2> temporary (!isVirtualReg() && regNum < PhysicalReg_GLUE_DVMDEX)
   3> glue variables: regNum >= PhysicalReg_GLUE_DVMDEX
*/
/** check whether a variable is a virtual register
 */
bool isVirtualReg(int type) {
    if((type & LowOpndRegType_virtual) != 0) return true;
    return false;
}
bool isTemporary(int type, int regNum) {
    if(!isVirtualReg(type) && regNum < PhysicalReg_GLUE_DVMDEX) return true;
    return false;
}

/** convert type defined in lowering module to type defined in register allocator
    in lowering module <type, isPhysical>
    in register allocator: LowOpndRegType_hard LowOpndRegType_virtual LowOpndRegType_scratch
*/
int convertType(int type, int reg, bool isPhysical) {
    int newType = type;
    if(isPhysical) newType |= LowOpndRegType_hard;
    if(isVirtualReg(type)) newType |= LowOpndRegType_virtual;
    else {
        /* reg number for a VR can exceed PhysicalReg_SCRATCH_1 */
        if(reg >= PhysicalReg_SCRATCH_1 && reg < PhysicalReg_GLUE_DVMDEX)
            newType |= LowOpndRegType_scratch;
    }
    return newType;
}

/** return the size of a variable
 */
OpndSize getRegSize(int type) {
    if((type & MASK_FOR_TYPE) == LowOpndRegType_xmm) return OpndSize_64;
    if((type & MASK_FOR_TYPE) == LowOpndRegType_fs) return OpndSize_64;
    /* for type _gp, _fs_s, _ss */
    return OpndSize_32;
}

/*
   Overlapping cases between two variables A and B
   layout for A,B   isAPartiallyOverlapB  isBPartiallyOverlapA
   1> |__|  |____|         OVERLAP_ALIGN        OVERLAP_B_COVER_A
      |__|  |____|
   2> |____|           OVERLAP_B_IS_LOW_OF_A    OVERLAP_B_COVER_LOW_OF_A
        |__|
   3> |____|           OVERLAP_B_IS_HIGH_OF_A   OVERLAP_B_COVER_HIGH_OF_A
      |__|
   4> |____|      OVERLAP_LOW_OF_A_IS_HIGH_OF_B OVERLAP_B_COVER_LOW_OF_A
         |____|
   5>    |____|   OVERLAP_HIGH_OF_A_IS_LOW_OF_B OVERLAP_B_COVER_HIGH_OF_A
      |____|
   6>   |__|           OVERLAP_A_IS_LOW_OF_B    OVERLAP_B_COVER_A
      |____|
   7> |__|             OVERLAP_A_IS_HIGH_OF_B   OVERLAP_B_COVER_A
      |____|
*/
/** determine the overlapping between variable B and A
*/
OverlapCase getBPartiallyOverlapA(int regB, LowOpndRegType tB, int regA, LowOpndRegType tA) {
    if(getRegSize(tA) == getRegSize(tB) && regA == regB) return OVERLAP_B_COVER_A;
    if(getRegSize(tA) == OpndSize_64 && getRegSize(tB) == OpndSize_32 && regA == regB) return OVERLAP_B_COVER_LOW_OF_A;
    if(getRegSize(tA) == OpndSize_64 && getRegSize(tB) == OpndSize_32 && regB == regA + 1) return OVERLAP_B_COVER_HIGH_OF_A;
    if(getRegSize(tA) == OpndSize_32 && getRegSize(tB) == OpndSize_64 && (regA == regB || regA == regB+1)) return OVERLAP_B_COVER_A;
    if(getRegSize(tB) == OpndSize_64 && getRegSize(tA) == OpndSize_64 && regA == regB+1) return OVERLAP_B_COVER_LOW_OF_A;
    if(getRegSize(tB) == OpndSize_64 && getRegSize(tA) == OpndSize_64 && regB == regA+1) return OVERLAP_B_COVER_HIGH_OF_A;
    return OVERLAP_NO;
}

/** determine overlapping between variable A and B
*/
OverlapCase getAPartiallyOverlapB(int regA, LowOpndRegType tA, int regB, LowOpndRegType tB) {
    if(getRegSize(tA) == getRegSize(tB) && regA == regB) return OVERLAP_ALIGN;
    if(getRegSize(tA) == OpndSize_64 && getRegSize(tB) == OpndSize_32 && regA == regB)
        return OVERLAP_B_IS_LOW_OF_A;
    if(getRegSize(tA) == OpndSize_64 && getRegSize(tB) == OpndSize_32 && regB == regA+1)
        return OVERLAP_B_IS_HIGH_OF_A;
    if(getRegSize(tB) == OpndSize_64 && getRegSize(tA) == OpndSize_64 && regA == regB+1)
        return OVERLAP_LOW_OF_A_IS_HIGH_OF_B;
    if(getRegSize(tB) == OpndSize_64 && getRegSize(tA) == OpndSize_64 && regB == regA+1)
        return OVERLAP_HIGH_OF_A_IS_LOW_OF_B;
    if(getRegSize(tA) == OpndSize_32 && getRegSize(tB) == OpndSize_64 && regA == regB)
        return OVERLAP_A_IS_LOW_OF_B;
    if(getRegSize(tA) == OpndSize_32 && getRegSize(tB) == OpndSize_64 && regA == regB+1)
        return OVERLAP_A_IS_HIGH_OF_B;
    return OVERLAP_NO;
}

/** determine whether variable A fully covers B
 */
bool isAFullyCoverB(int regA, LowOpndRegType tA, int regB, LowOpndRegType tB) {
    if(getRegSize(tB) == OpndSize_32) return true;
    if(getRegSize(tA) == getRegSize(tB) && regA == regB) return true;
    return false;
}

/*
   RegAccessType accessType
   1> DefOrUse.accessType
      can only be D(VR), L(low part of VR), H(high part of VR), N(none)
      for def, it means which part of the VR is live
      for use, it means which part of the VR comes from the def
   2> VirtualRegInfo.accessType
      for currentInfo, it can only be a combination of U & D
      for entries in infoBasicBlock, it can be a combination of U & D|L|H
*/

/*
   Key data structures used:
   1> BasicBlock_O1
      VirtualRegInfo infoBasicBlock[]
      DefUsePair* defUseTable
      XferPoint xferPoints[]
   2> MemoryVRInfo memVRTable[]
      LiveRange* ranges
   3> compileTableEntry compileTable[]
   4> VirtualRegInfo
      DefOrUse reachingDefs[3]
   5> DefUsePair, LiveRange
*/

//! one entry for each variable used

//! a variable can be virtual register, or a temporary (can be hard-coded)
compileTableEntry compileTable[COMPILE_TABLE_SIZE];
int num_compile_entries;
//! tables to save the states of register allocation
regAllocStateEntry1 stateTable1_1[COMPILE_TABLE_SIZE];
regAllocStateEntry1 stateTable1_2[COMPILE_TABLE_SIZE];
regAllocStateEntry1 stateTable1_3[COMPILE_TABLE_SIZE];
regAllocStateEntry1 stateTable1_4[COMPILE_TABLE_SIZE];
regAllocStateEntry2 stateTable2_1[COMPILE_TABLE_SIZE];
regAllocStateEntry2 stateTable2_2[COMPILE_TABLE_SIZE];
regAllocStateEntry2 stateTable2_3[COMPILE_TABLE_SIZE];
regAllocStateEntry2 stateTable2_4[COMPILE_TABLE_SIZE];

//! array of VirtualRegInfo to store VRs accessed by a single bytecode
VirtualRegInfo infoByteCode[MAX_REG_PER_BYTECODE];
int num_regs_per_bytecode;
//! array of TempRegInfo to store temporaries accessed by a single bytecode
TempRegInfo infoByteCodeTemp[MAX_TEMP_REG_PER_BYTECODE];
int num_temp_regs_per_bytecode;
//! array of MemoryVRInfo to store whether a VR is in memory
#define NUM_MEM_VR_ENTRY 140
MemoryVRInfo memVRTable[NUM_MEM_VR_ENTRY];
int num_memory_vr;

CompilationUnit* currentUnit = NULL;

//! the current basic block
BasicBlock_O1* currentBB = NULL;
//! array of RegisterInfo for all the physical registers
RegisterInfo allRegs[PhysicalReg_GLUE+1]; //initialized in codeGen

VirtualRegInfo currentInfo;
VirtualRegInfo tmpInfo;

//! this array says whether a spill location is used (0 means not used, 1 means used)
int spillIndexUsed[MAX_SPILL_JIT_IA];
int indexForGlue = -1;

int num_bbs_for_method;
//! array of basic blocks in a method in program order
BasicBlock_O1* method_bbs_sorted[MAX_NUM_BBS_PER_METHOD];
//! the entry basic block
BasicBlock_O1* bb_entry;
int pc_start = -1;
int pc_end = -1;

//!array of PCs for exception handlers
int exceptionHandlers[10];
int num_exception_handlers;

bool canSpillReg[PhysicalReg_Null]; //physical registers that should not be spilled
int inGetVR_num = -1;
int inGetVR_type;

///////////////////////////////////////////////////////////////////////////////
// FORWARD FUNCTION DECLARATION
void addExceptionHandler(s4 tmp);

int createCFG(Method* method);
int collectInfoOfBasicBlock(Method* method, BasicBlock_O1* bb);
void dumpVirtualInfoOfBasicBlock(BasicBlock_O1* bb);
void setTypeOfVR();
void insertGlueReg();
void dumpVirtualInfoOfMethod();
int codeGenBasicBlock(const Method* method, BasicBlock_O1* bb);

//used in collectInfoOfBasicBlock: getVirtualRegInfo
int mergeEntry2(BasicBlock_O1* bb);
int sortAllocConstraint(RegAllocConstraint* allocConstraints,
                        RegAllocConstraint* allocConstraintsSorted, bool fromHighToLow);

//used in codeGenBasicBlock
void insertFromVirtualInfo(BasicBlock_O1* bb, int k); //update compileTable
void insertFromTempInfo(int k); //update compileTable
int updateXferPoints();
void updateLiveTable();
void printDefUseTable();
bool isFirstOfHandler(BasicBlock_O1* bb);

//used in mergeEntry2
//following functions will not update global data structure
RegAccessType mergeAccess2(RegAccessType A, RegAccessType B, OverlapCase isBPartiallyOverlapA);
RegAccessType updateAccess1(RegAccessType A, OverlapCase isAPartiallyOverlapB); //will not update global data structure
RegAccessType updateAccess2(RegAccessType C1, RegAccessType C2);
RegAccessType updateAccess3(RegAccessType C, RegAccessType B);

void updateDefUseTable();
void updateReachingDefA(int indexToA, OverlapCase isBPartiallyOverlapA);
void updateReachingDefB1(int indexToA);
void updateReachingDefB2();
void updateReachingDefB3();

RegAccessType insertAUse(DefUsePair* ptr, int offsetPC, int regNum, LowOpndRegType physicalType);
DefUsePair* insertADef(int offsetPC, int regNum, LowOpndRegType pType, RegAccessType rType);
RegAccessType insertDefUsePair(int reachingDefIndex);

//used in updateXferPoints
int fakeUsageAtEndOfBB(BasicBlock_O1* bb);
void insertLoadXfer(int offset, int regNum, LowOpndRegType pType);
int searchMemTable(int regNum);
void mergeLiveRange(int tableIndex, int rangeStart, int rangeEnd);
//used in updateLiveTable
RegAccessType setAccessTypeOfUse(OverlapCase isDefPartiallyOverlapUse, RegAccessType reachingDefLive);
DefUsePair* searchDefUseTable(int offsetPC, int regNum, LowOpndRegType pType);
void insertAccess(int tableIndex, LiveRange* startP, int rangeStart);

//register allocation
int spillLogicalReg(int spill_index, bool updateTable);

/** check whether the current bytecode is IF or GOTO or SWITCH
 */
bool isCurrentByteCodeJump() {
    u2 inst_op = INST_INST(inst);
    if(inst_op == OP_IF_EQ || inst_op == OP_IF_NE || inst_op == OP_IF_LT ||
       inst_op == OP_IF_GE || inst_op == OP_IF_GT || inst_op == OP_IF_LE) return true;
    if(inst_op == OP_IF_EQZ || inst_op == OP_IF_NEZ || inst_op == OP_IF_LTZ ||
       inst_op == OP_IF_GEZ || inst_op == OP_IF_GTZ || inst_op == OP_IF_LEZ) return true;
    if(inst_op == OP_GOTO || inst_op == OP_GOTO_16 || inst_op == OP_GOTO_32) return true;
    if(inst_op == OP_PACKED_SWITCH || inst_op == OP_SPARSE_SWITCH) return true;
    return false;
}

/* this function is called before code generation of basic blocks
   initialize data structure allRegs, which stores information for each physical register,
   whether it is used, when it was last freed, whether it is callee-saved */
void initializeAllRegs() {
    int k;
    for(k = PhysicalReg_EAX; k <= PhysicalReg_EBP; k++) {
        allRegs[k].physicalReg = (PhysicalReg) k;
        if(k == PhysicalReg_EDI || k == PhysicalReg_ESP || k == PhysicalReg_EBP)
            allRegs[k].isUsed = true;
        else {
            allRegs[k].isUsed = false;
            allRegs[k].freeTimeStamp = -1;
        }
        if(k == PhysicalReg_EBX || k == PhysicalReg_EBP || k == PhysicalReg_ESI || k == PhysicalReg_EDI)
            allRegs[k].isCalleeSaved = true;
        else
            allRegs[k].isCalleeSaved = false;
    }
    for(k = PhysicalReg_XMM0; k <= PhysicalReg_XMM7; k++) {
        allRegs[k].physicalReg = (PhysicalReg) k;
        allRegs[k].isUsed = false;
        allRegs[k].freeTimeStamp = -1;
        allRegs[k].isCalleeSaved = false;
    }
}

/** sync up allRegs (isUsed & freeTimeStamp) with compileTable
    global data: RegisterInfo allRegs[PhysicalReg_Null]
    update allRegs[EAX to XMM7] except EDI,ESP,EBP
    update RegisterInfo.isUsed & RegisterInfo.freeTimeStamp
        if the physical register was used and is not used now
*/
void syncAllRegs() {
    int k, k2;
    for(k = PhysicalReg_EAX; k <= PhysicalReg_XMM7; k++) {
        if(k == PhysicalReg_EDI || k == PhysicalReg_ESP || k == PhysicalReg_EBP)
            continue;
        //check whether the physical register is used by any logical register
        bool stillUsed = false;
        for(k2 = 0; k2 < num_compile_entries; k2++) {
            if(compileTable[k2].physicalReg == k) {
                stillUsed = true;
                break;
            }
        }
        if(stillUsed && !allRegs[k].isUsed) {
            allRegs[k].isUsed = true;
        }
        if(!stillUsed && allRegs[k].isUsed) {
            allRegs[k].isUsed = false;
            allRegs[k].freeTimeStamp = lowOpTimeStamp;
        }
    }
    return;
}

//!sync up spillIndexUsed with compileTable

//!
void updateSpillIndexUsed() {
    int k;
    for(k = 0; k <= MAX_SPILL_JIT_IA-1; k++) spillIndexUsed[k] = 0;
    for(k = 0; k < num_compile_entries; k++) {
        if(isVirtualReg(compileTable[k].physicalType)) continue;
        if(compileTable[k].spill_loc_index >= 0) {
            if(compileTable[k].spill_loc_index > 4*(MAX_SPILL_JIT_IA-1))
                ALOGE("spill_loc_index is wrong for entry %d: %d",
                      k, compileTable[k].spill_loc_index);
            spillIndexUsed[compileTable[k].spill_loc_index >> 2] = 1;
        }
    }
}

/* free memory used in all basic blocks */
void freeCFG() {
    int k;
    for(k = 0; k < num_bbs_for_method; k++) {
        /* free defUseTable for method_bbs_sorted[k] */
        DefUsePair* ptr = method_bbs_sorted[k]->defUseTable;
        while(ptr != NULL) {
            DefUsePair* tmp = ptr->next;
            /* free ptr->uses */
            DefOrUseLink* ptrUse = ptr->uses;
            while(ptrUse != NULL) {
                DefOrUseLink* tmp2 = ptrUse->next;
                free(ptrUse);
                ptrUse = tmp2;
            }
            free(ptr);
            ptr = tmp;
        }
        free(method_bbs_sorted[k]);
    }
}

/* update compileTable.physicalReg, compileTable.spill_loc_index & allRegs.isUsed
   for glue-related variables, they do not exist
       not in a physical register (physicalReg is Null)
       not in a spilled memory location (spill_loc_index is -1)
*/
void initializeRegStateOfBB(BasicBlock_O1* bb) {
    //for GLUE variables, do not exist
    int k;
    for(k = 0; k < num_compile_entries; k++) {
        /* trace-based JIT: there is no VR with GG type */
        if(isVirtualReg(compileTable[k].physicalType) && compileTable[k].gType == GLOBALTYPE_GG) {
            if(bb->bb_index > 0) { //non-entry block
                if(isFirstOfHandler(bb)) {
                    /* at the beginning of an exception handler, GG VR is in the interpreted stack */
                    compileTable[k].physicalReg = PhysicalReg_Null;
#ifdef DEBUG_COMPILE_TABLE
                    ALOGI("at the first basic block of an exception handler, GG VR %d type %d is in memory",
                          compileTable[k].regNum, compileTable[k].physicalType);
#endif
                } else {
                    if(compileTable[k].physicalReg == PhysicalReg_Null) {
                        /* GG VR is in a specific physical register */
                        compileTable[k].physicalReg = compileTable[k].physicalReg_prev;
                    }
                    int tReg = compileTable[k].physicalReg;
                    allRegs[tReg].isUsed = true;
#ifdef DEBUG_REG_USED
                    ALOGI("REGALLOC: physical reg %d is used by a GG VR %d %d at beginning of BB", tReg, compileTable[k].regNum, compileTable[k].physicalType);
#endif
                }
            } //non-entry block
        } //if GG VR
        if(compileTable[k].regNum != PhysicalReg_GLUE &&
           compileTable[k].regNum >= PhysicalReg_GLUE_DVMDEX) {
            /* glue related registers */
            compileTable[k].physicalReg = PhysicalReg_Null;
            compileTable[k].spill_loc_index = -1;
        }
    }
}

/* update memVRTable[].nullCheckDone */
void initializeNullCheck(int indexToMemVR) {
    bool found = false;
#ifdef GLOBAL_NULLCHECK_OPT
    /* search nullCheck_inB of the current Basic Block */
    for(k = 0; k < nullCheck_inSize[currentBB->bb_index2]; k++) {
        if(nullCheck_inB[currentBB->bb_index2][k] == memVRTable[indexToMemVR].regNum) {
            found = true;
            break;
        }
    }
#endif
    memVRTable[indexToMemVR].nullCheckDone = found;
}

/* initialize memVRTable */
void initializeMemVRTable() {
    num_memory_vr = 0;
    int k;
    for(k = 0; k < num_compile_entries; k++) {
        if(!isVirtualReg(compileTable[k].physicalType)) continue;
        /* VRs in compileTable */
        bool setToInMemory = (compileTable[k].physicalReg == PhysicalReg_Null);
        int regNum = compileTable[k].regNum;
        OpndSize sizeVR = getRegSize(compileTable[k].physicalType);
        /* search memVRTable for the VR in compileTable */
        int kk;
        int indexL = -1;
        int indexH = -1;
        for(kk = 0; kk < num_memory_vr; kk++) {
            if(memVRTable[kk].regNum == regNum) {
                indexL = kk;
                continue;
            }
            if(memVRTable[kk].regNum == regNum+1 && sizeVR == OpndSize_64) {
                indexH = kk;
                continue;
            }
        }
        if(indexL < 0) {
            /* the low half of VR is not in memVRTable
               add an entry for the low half in memVRTable */
            if(num_memory_vr >= NUM_MEM_VR_ENTRY) {
                ALOGE("exceeds size of memVRTable");
                dvmAbort();
            }
            memVRTable[num_memory_vr].regNum = regNum;
            memVRTable[num_memory_vr].inMemory = setToInMemory;
            initializeNullCheck(num_memory_vr); //set nullCheckDone
            memVRTable[num_memory_vr].boundCheck.checkDone = false;
            memVRTable[num_memory_vr].num_ranges = 0;
            memVRTable[num_memory_vr].ranges = NULL;
            memVRTable[num_memory_vr].delayFreeFlags = VRDELAY_NONE;
            num_memory_vr++;
        }
        if(sizeVR == OpndSize_64 && indexH < 0) {
            /* the high half of VR is not in memVRTable
               add an entry for the high half in memVRTable */
            if(num_memory_vr >= NUM_MEM_VR_ENTRY) {
                ALOGE("exceeds size of memVRTable");
                dvmAbort();
            }
            memVRTable[num_memory_vr].regNum = regNum+1;
            memVRTable[num_memory_vr].inMemory = setToInMemory;
            initializeNullCheck(num_memory_vr);
            memVRTable[num_memory_vr].boundCheck.checkDone = false;
            memVRTable[num_memory_vr].num_ranges = 0;
            memVRTable[num_memory_vr].ranges = NULL;
            memVRTable[num_memory_vr].delayFreeFlags = VRDELAY_NONE;
            num_memory_vr++;
        }
    }
}

/* create a O1 basic block from basic block constructed in JIT MIR */
BasicBlock_O1* createBasicBlockO1(BasicBlock* bb) {
    BasicBlock_O1* bb1 = createBasicBlock(0, -1);
    bb1->jitBasicBlock = bb;
    return bb1;
}

/* a basic block in JIT MIR can contain bytecodes that are not in program order
   for example, a "goto" bytecode will be followed by the goto target */
void preprocessingBB(BasicBlock* bb) {
    currentBB = createBasicBlockO1(bb);
    /* initialize currentBB->allocConstraints */
    int ii;
    for(ii = 0; ii < 8; ii++) {
        currentBB->allocConstraints[ii].physicalReg = (PhysicalReg)ii;
        currentBB->allocConstraints[ii].count = 0;
    }
    collectInfoOfBasicBlock(currentMethod, currentBB);
#ifdef DEBUG_COMPILE_TABLE
    dumpVirtualInfoOfBasicBlock(currentBB);
#endif
    currentBB = NULL;
}

void preprocessingTrace() {
    int k, k2, k3, jj;
    /* this is a simplified verson of setTypeOfVR()
        all VRs are assumed to be GL, no VR will be GG
    */
    for(k = 0; k < num_bbs_for_method; k++)
        for(jj = 0; jj < method_bbs_sorted[k]->num_regs; jj++)
            method_bbs_sorted[k]->infoBasicBlock[jj].gType = GLOBALTYPE_GL;

    /* insert a glue-related register GLUE_DVMDEX to compileTable */
    insertGlueReg();

    int compile_entries_old = num_compile_entries;
    for(k2 = 0; k2 < num_bbs_for_method; k2++) {
        currentBB = method_bbs_sorted[k2];
        /* update compileTable with virtual register from currentBB */
        for(k3 = 0; k3 < currentBB->num_regs; k3++) {
            insertFromVirtualInfo(currentBB, k3);
        }

        /* for each GL|GG type VR, insert fake usage at end of basic block to keep it live */
        int offsetPC_back = offsetPC;
        offsetPC = PC_FOR_END_OF_BB;
        for(k = 0; k < num_compile_entries; k++) {
            currentInfo.regNum = compileTable[k].regNum;
            currentInfo.physicalType = (LowOpndRegType)compileTable[k].physicalType;
            if(isVirtualReg(compileTable[k].physicalType) &&
               compileTable[k].gType == GLOBALTYPE_GL) {
                /* update defUseTable by assuming a fake usage at END of a basic block for variable @ currentInfo */
                fakeUsageAtEndOfBB(currentBB);
            }
            if(isVirtualReg(compileTable[k].physicalType) &&
               compileTable[k].gType == GLOBALTYPE_GG) {
                fakeUsageAtEndOfBB(currentBB);
            }
        }
        offsetPC = offsetPC_back;
        num_compile_entries = compile_entries_old;
    }
    /* initialize data structure allRegs */
    initializeAllRegs();
#ifdef DEBUG_COMPILE_TABLE
    dumpCompileTable();
#endif
    currentBB = NULL;
}

void printJitTraceInfoAtRunTime(const Method* method, int offset) {
    ALOGI("execute trace for %s%s at offset %x", method->clazz->descriptor, method->name, offset);
}

void startOfTraceO1(const Method* method, LowOpBlockLabel* labelList, int exceptionBlockId, CompilationUnit *cUnit) {
    num_exception_handlers = 0;
    num_compile_entries = 0;
    currentBB = NULL;
    pc_start = -1;
    bb_entry = NULL;
    num_bbs_for_method = 0;
    currentUnit = cUnit;
    lowOpTimeStamp = 0;

// dumpDebuggingInfo is gone in CompilationUnit struct
#if 0
    /* add code to dump debugging information */
    if(cUnit->dumpDebuggingInfo) {
        move_imm_to_mem(OpndSize_32, cUnit->startOffset, -4, PhysicalReg_ESP, true); //2nd argument: offset
        move_imm_to_mem(OpndSize_32, (int)currentMethod, -8, PhysicalReg_ESP, true); //1st argument: method
        load_effective_addr(-8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);

        typedef void (*vmHelper)(const Method*, int);
        vmHelper funcPtr = printJitTraceInfoAtRunTime;
        move_imm_to_reg(OpndSize_32, (int)funcPtr, PhysicalReg_ECX, true);
        call_reg(PhysicalReg_ECX, true);

        load_effective_addr(8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    }
#endif
}


/* Code generation for a basic block defined for JIT
   We have two data structures for a basic block:
       BasicBlock defined in vm/compiler by JIT
       BasicBlock_O1 defined in o1 */
int codeGenBasicBlockJit(const Method* method, BasicBlock* bb) {
    /* search method_bbs_sorted to find the O1 basic block corresponding to bb */
    int k;
    for(k = 0; k < num_bbs_for_method; k++) {
        if(method_bbs_sorted[k]->jitBasicBlock == bb) {
            lowOpTimeStamp = 0; //reset time stamp at start of a basic block
            currentBB = method_bbs_sorted[k];
            int cg_ret = codeGenBasicBlock(method, currentBB);
            currentBB = NULL;
            return cg_ret;
        }
    }
    ALOGE("can't find the corresponding O1 basic block for id %d type %d",
         bb->id, bb->blockType);
    return -1;
}
void endOfBasicBlock(BasicBlock* bb) {
    isScratchPhysical = true;
    currentBB = NULL;
}
void endOfTraceO1() {
     freeCFG();
}

/** entry point to collect information about virtual registers used in a basic block
    Initialize data structure BasicBlock_O1
    The usage information of virtual registers is stoerd in bb->infoBasicBlock

    Global variables accessed: offsetPC, rPC
*/
int collectInfoOfBasicBlock(Method* method, BasicBlock_O1* bb) {
    bb->num_regs = 0;
    bb->num_defs = 0;
    bb->defUseTable = NULL;
    bb->defUseTail = NULL;
    u2* rPC_start = (u2*)method->insns;
    int kk;
    bb->endsWithReturn = false;
    bb->hasAccessToGlue = false;

    MIR* mir;
    int seqNum = 0;
    /* traverse the MIR in basic block
       sequence number is used to make sure next bytecode will have a larger sequence number */
    for(mir = bb->jitBasicBlock->firstMIRInsn; mir; mir = mir->next) {
        offsetPC = seqNum;
        mir->seqNum = seqNum++;
        rPC = rPC_start + mir->offset;
#ifdef WITH_JIT_INLINING
        if(mir->dalvikInsn.opcode >= kMirOpFirst &&
           mir->dalvikInsn.opcode != kMirOpCheckInlinePrediction) continue;
        if(ir->dalvikInsn.opcode == kMirOpCheckInlinePrediction) { //TODO
        }
#else
        if(mir->dalvikInsn.opcode >= kNumPackedOpcodes) continue;
#endif
        inst = FETCH(0);
        u2 inst_op = INST_INST(inst);
        /* update bb->hasAccessToGlue */
        if((inst_op >= OP_MOVE_RESULT && inst_op <= OP_RETURN_OBJECT) ||
           (inst_op >= OP_MONITOR_ENTER && inst_op <= OP_INSTANCE_OF) ||
           (inst_op == OP_FILLED_NEW_ARRAY) ||
           (inst_op == OP_FILLED_NEW_ARRAY_RANGE) ||
           (inst_op == OP_THROW) ||
           (inst_op >= OP_INVOKE_VIRTUAL && inst_op <= OP_INVOKE_INTERFACE_RANGE) ||
           (inst_op >= OP_THROW_VERIFICATION_ERROR &&
            inst_op <= OP_EXECUTE_INLINE_RANGE) ||
           (inst_op >= OP_INVOKE_VIRTUAL_QUICK && inst_op <= OP_INVOKE_SUPER_QUICK_RANGE))
            bb->hasAccessToGlue = true;
        /* update bb->endsWithReturn */
        if(inst_op == OP_RETURN_VOID || inst_op == OP_RETURN || inst_op == OP_RETURN_VOID_BARRIER ||
           inst_op == OP_RETURN_OBJECT || inst_op == OP_RETURN_WIDE)
            bb->endsWithReturn = true;

        /* get virtual register usage in current bytecode */
        getVirtualRegInfo(infoByteCode);
        int num_regs = num_regs_per_bytecode;
        for(kk = 0; kk < num_regs; kk++) {
            currentInfo = infoByteCode[kk];
#ifdef DEBUG_MERGE_ENTRY
            ALOGI("call mergeEntry2 at offsetPC %x kk %d VR %d %d", offsetPC, kk,
                  currentInfo.regNum, currentInfo.physicalType);
#endif
            mergeEntry2(bb); //update defUseTable of the basic block
        }

        //dumpVirtualInfoOfBasicBlock(bb);
    }//for each bytecode

    bb->pc_end = seqNum;

    //sort allocConstraints of each basic block
    for(kk = 0; kk < bb->num_regs; kk++) {
#ifdef DEBUG_ALLOC_CONSTRAINT
        ALOGI("sort virtual reg %d type %d -------", bb->infoBasicBlock[kk].regNum,
              bb->infoBasicBlock[kk].physicalType);
#endif
        sortAllocConstraint(bb->infoBasicBlock[kk].allocConstraints,
                            bb->infoBasicBlock[kk].allocConstraintsSorted, true);
    }
#ifdef DEBUG_ALLOC_CONSTRAINT
    ALOGI("sort constraints for BB %d --------", bb->bb_index);
#endif
    sortAllocConstraint(bb->allocConstraints, bb->allocConstraintsSorted, false);
    return 0;
}

/** entry point to generate native code for a O1 basic block
    There are 3 kinds of virtual registers in a O1 basic block:
    1> L VR: local within the basic block
    2> GG VR: is live in other basic blocks,
              its content is in a pre-defined GPR at the beginning of a basic block
    3> GL VR: is live in other basic blocks,
              its content is in the interpreted stack at the beginning of a basic block
    compileTable is updated with infoBasicBlock at the start of the basic block;
    Before lowering each bytecode, compileTable is updated with infoByteCodeTemp;
    At end of the basic block, right before the jump instruction, handles constant VRs and GG VRs
*/
int codeGenBasicBlock(const Method* method, BasicBlock_O1* bb) {
    /* we assume at the beginning of each basic block,
       all GL VRs reside in memory and all GG VRs reside in predefined physical registers,
       so at the end of a basic block, recover a spilled GG VR, store a GL VR to memory */
    /* update compileTable with entries in bb->infoBasicBlock */
    int k;
    for(k = 0; k < bb->num_regs; k++) {
        insertFromVirtualInfo(bb, k);
    }
    updateXferPoints(); //call fakeUsageAtEndOfBB
#ifdef DEBUG_REACHING_DEF
    printDefUseTable();
#endif
#ifdef DSE_OPT
    removeDeadDefs();
    printDefUseTable();
#endif
    //clear const section of compileTable
    for(k = 0; k < num_compile_entries; k++) compileTable[k].isConst = false;
    num_const_vr = 0;
#ifdef DEBUG_COMPILE_TABLE
    ALOGI("At start of basic block %d (num of VRs %d) -------", bb->bb_index, bb->num_regs);
    dumpCompileTable();
#endif
    initializeRegStateOfBB(bb);
    initializeMemVRTable();
    updateLiveTable();
    freeReg(true);  //before code gen of a basic block, also called at end of a basic block?
#ifdef DEBUG_COMPILE_TABLE
    ALOGI("At start of basic block %d (num of VRs %d) -------", bb->bb_index, bb->num_regs);
#endif

    u2* rPC_start = (u2*)method->insns;
    bool lastByteCodeIsJump = false;
    MIR* mir;
    for(mir = bb->jitBasicBlock->firstMIRInsn; mir; mir = mir->next) {
        offsetPC = mir->seqNum;
        rPC = rPC_start + mir->offset;
#ifdef WITH_JIT_INLINING
        if(mir->dalvikInsn.opcode >= kMirOpFirst &&
           mir->dalvikInsn.opcode != kMirOpCheckInlinePrediction) {
#else
        if(mir->dalvikInsn.opcode >= kNumPackedOpcodes) {
#endif
            handleExtendedMIR(currentUnit, mir);
            continue;
        }

        inst = FETCH(0);
        //before handling a bytecode, import info of temporary registers to compileTable including refCount
        num_temp_regs_per_bytecode = getTempRegInfo(infoByteCodeTemp);
        for(k = 0; k < num_temp_regs_per_bytecode; k++) {
            if(infoByteCodeTemp[k].versionNum > 0) continue;
            insertFromTempInfo(k);
        }
        startNativeCode(-1, -1);
        for(k = 0; k <= MAX_SPILL_JIT_IA-1; k++) spillIndexUsed[k] = 0;
        //update spillIndexUsed if a glue variable was spilled
        for(k = 0; k < num_compile_entries; k++) {
            if(compileTable[k].regNum >= PhysicalReg_GLUE_DVMDEX) {
                if(compileTable[k].spill_loc_index >= 0)
                    spillIndexUsed[compileTable[k].spill_loc_index >> 2] = 1;
            }
        }
#ifdef DEBUG_COMPILE_TABLE
        ALOGI("compile table size after importing temporary info %d", num_compile_entries);
        ALOGI("before one bytecode %d (num of VRs %d) -------", bb->bb_index, bb->num_regs);
#endif
        //set isConst to true for CONST & MOVE MOVE_OBJ?
        //clear isConst to true for MOVE, MOVE_OBJ, MOVE_RESULT, MOVE_EXCEPTION ...
        bool isConst = getConstInfo(bb); //will reset isConst if a VR is updated by the bytecode
        bool isDeadStmt = false;
#ifdef DSE_OPT
        for(k = 0; k < num_dead_pc; k++) {
            if(deadPCs[k] == offsetPC) {
                isDeadStmt = true;
                break;
            }
        }
#endif
        getVirtualRegInfo(infoByteCode);
        //call something similar to mergeEntry2, but only update refCount
        //clear refCount
        for(k = 0; k < num_regs_per_bytecode; k++) {
            int indexT = searchCompileTable(LowOpndRegType_virtual | infoByteCode[k].physicalType,
                                            infoByteCode[k].regNum);
            if(indexT >= 0)
                compileTable[indexT].refCount = 0;
        }
        for(k = 0; k < num_regs_per_bytecode; k++) {
            int indexT = searchCompileTable(LowOpndRegType_virtual | infoByteCode[k].physicalType,
                                            infoByteCode[k].regNum);
            if(indexT >= 0)
                compileTable[indexT].refCount += infoByteCode[k].refCount;
        } //for k
#ifdef DSE_OPT
        if(isDeadStmt) { //search compileTable
            getVirtualRegInfo(infoByteCode);
#ifdef DEBUG_DSE
            ALOGI("DSE: stmt at offsetPC %d is dead", offsetPC);
#endif
            for(k = 0; k < num_regs_per_bytecode; k++) {
                int indexT = searchCompileTable(LowOpndRegType_virtual | infoByteCode[k].physicalType,
                                                infoByteCode[k].regNum);
                if(indexT >= 0)
                    compileTable[indexT].refCount -= infoByteCode[k].refCount;
            }
        }
#endif
        lastByteCodeIsJump = false;
        if(!isConst && !isDeadStmt)  //isDeadStmt is false when DSE_OPT is not enabled
        {
#ifdef DEBUG_COMPILE_TABLE
            dumpCompileTable();
#endif
            globalShortMap = NULL;
            if(isCurrentByteCodeJump()) lastByteCodeIsJump = true;
            //lowerByteCode will call globalVREndOfBB if it is jump
            int retCode = lowerByteCodeJit(method, rPC, mir);
            if(gDvmJit.codeCacheByteUsed + (stream - streamStart) +
                 CODE_CACHE_PADDING > gDvmJit.codeCacheSize) {
                 ALOGE("JIT code cache full");
                 gDvmJit.codeCacheFull = true;
                 return -1;
            }

            if (retCode == 1) {
                // We always fall back to the interpreter for OP_INVOKE_OBJECT_INIT_RANGE,
                // but any other failure is unexpected and should be logged.
                if (mir->dalvikInsn.opcode != OP_INVOKE_OBJECT_INIT_RANGE) {
                    ALOGE("JIT couldn't compile %s%s dex_pc=%d opcode=%d",
                          method->clazz->descriptor,
                          method->name,
                          offsetPC,
                          mir->dalvikInsn.opcode);
                }
                return -1;
            }
            updateConstInfo(bb);
            freeShortMap();
            if(retCode < 0) {
                ALOGE("error in lowering the bytecode");
                return retCode;
            }
            freeReg(true); //may dump GL VR to memory (this is necessary)
            //after each bytecode, make sure non-VRs have refCount of zero
            for(k = 0; k < num_compile_entries; k++) {
                if(isTemporary(compileTable[k].physicalType, compileTable[k].regNum)) {
#ifdef PRINT_WARNING
                    if(compileTable[k].refCount > 0) {
                        ALOGW("refCount for a temporary reg %d %d is %d after a bytecode", compileTable[k].regNum, compileTable[k].physicalType, compileTable[k].refCount);
                    }
#endif
                    compileTable[k].refCount = 0;
                }
            }
        } else { //isConst || isDeadStmt
            //if this bytecode is the target of a jump, the mapFromBCtoNCG should be updated
            offsetNCG = stream - streamMethodStart;
            mapFromBCtoNCG[offsetPC] = offsetNCG;
#ifdef DEBUG_COMPILE_TABLE
            ALOGI("this bytecode generates a constant and has no side effect");
#endif
            freeReg(true); //may dump GL VR to memory (this is necessary)
        }
#ifdef DEBUG_COMPILE_TABLE
        ALOGI("after one bytecode BB %d (num of VRs %d)", bb->bb_index, bb->num_regs);
#endif
    }//for each bytecode
#ifdef DEBUG_COMPILE_TABLE
    dumpCompileTable();
#endif
    if(!lastByteCodeIsJump) constVREndOfBB();
    //at end of a basic block, get spilled GG VR & dump GL VR
    if(!lastByteCodeIsJump) globalVREndOfBB(method);
    //remove entries for temporary registers, L VR and GL VR
    int jj;
    for(k = 0; k < num_compile_entries; ) {
        bool removeEntry = false;
        if(isVirtualReg(compileTable[k].physicalType) && compileTable[k].gType != GLOBALTYPE_GG) {
            removeEntry = true;
        }
        if(isTemporary(compileTable[k].physicalType, compileTable[k].regNum))
            removeEntry = true;
        if(removeEntry) {
#ifdef PRINT_WARNING
            if(compileTable[k].refCount > 0)
                ALOGW("refCount for REG %d %d is %d at end of a basic block", compileTable[k].regNum, compileTable[k].physicalType, compileTable[k].refCount);
#endif
            compileTable[k].refCount = 0;
            for(jj = k+1; jj < num_compile_entries; jj++) {
                compileTable[jj-1] = compileTable[jj];
            }
            num_compile_entries--;
        } else {
            k++;
        }
    }
    freeReg(true);
    //free LIVE TABLE
    for(k = 0; k < num_memory_vr; k++) {
        LiveRange* ptr2 = memVRTable[k].ranges;
        while(ptr2 != NULL) {
            LiveRange* tmpP = ptr2->next;
            free(ptr2->accessPC);
            free(ptr2);
            ptr2 = tmpP;
        }
    }
#ifdef DEBUG_COMPILE_TABLE
    ALOGI("At end of basic block -------");
    dumpCompileTable();
#endif
    return 0;
}

/** update infoBasicBlock & defUseTable
    input: currentInfo
    side effect: update currentInfo.reachingDefs

    update entries in infoBasicBlock by calling updateReachingDefA
    if there is no entry in infoBasicBlock for B, an entry will be created and inserted to infoBasicBlock

    defUseTable is updated to account for the access at currentInfo
    if accessType of B is U or UD, we call updateReachingDefB to update currentInfo.reachingDefs
        in order to correctly insert the usage to defUseTable
*/
int mergeEntry2(BasicBlock_O1* bb) {
    LowOpndRegType typeB = currentInfo.physicalType;
    int regB = currentInfo.regNum;
    int jj, k;
    int jjend = bb->num_regs;
    bool isMerged = false;
    bool hasAlias = false;
    OverlapCase isBPartiallyOverlapA, isAPartiallyOverlapB;
    RegAccessType tmpType = REGACCESS_N;
    currentInfo.num_reaching_defs = 0;

    /* traverse variable A in infoBasicBlock */
    for(jj = 0; jj < jjend; jj++) {
        int regA = bb->infoBasicBlock[jj].regNum;
        LowOpndRegType typeA = bb->infoBasicBlock[jj].physicalType;
        isBPartiallyOverlapA = getBPartiallyOverlapA(regB, typeB, regA, typeA);
        isAPartiallyOverlapB = getAPartiallyOverlapB(regA, typeA, regB, typeB);
        if(regA == regB && typeA == typeB) {
            /* variable A and B are aligned */
            bb->infoBasicBlock[jj].accessType = mergeAccess2(bb->infoBasicBlock[jj].accessType, currentInfo.accessType,
                                                             OVERLAP_B_COVER_A);
            bb->infoBasicBlock[jj].refCount += currentInfo.refCount;
            /* copy reaching defs of variable B from variable A */
            currentInfo.num_reaching_defs = bb->infoBasicBlock[jj].num_reaching_defs;
            for(k = 0; k < currentInfo.num_reaching_defs; k++)
                currentInfo.reachingDefs[k] = bb->infoBasicBlock[jj].reachingDefs[k];
            updateDefUseTable(); //use currentInfo to update defUseTable
            updateReachingDefA(jj, OVERLAP_B_COVER_A); //update reachingDefs of A
            isMerged = true;
            hasAlias = true;
            if(typeB == LowOpndRegType_gp) {
                //merge allocConstraints
                for(k = 0; k < 8; k++) {
                    bb->infoBasicBlock[jj].allocConstraints[k].count += currentInfo.allocConstraints[k].count;
                }
            }
        }
        else if(isBPartiallyOverlapA != OVERLAP_NO) {
            tmpType = updateAccess2(tmpType, updateAccess1(bb->infoBasicBlock[jj].accessType, isAPartiallyOverlapB));
            bb->infoBasicBlock[jj].accessType = mergeAccess2(bb->infoBasicBlock[jj].accessType, currentInfo.accessType,
                                                             isBPartiallyOverlapA);
#ifdef DEBUG_MERGE_ENTRY
            ALOGI("update accessType in case 2: VR %d %d accessType %d", regA, typeA, bb->infoBasicBlock[jj].accessType);
#endif
            hasAlias = true;
            if(currentInfo.accessType == REGACCESS_U || currentInfo.accessType == REGACCESS_UD) {
                /* update currentInfo.reachingDefs */
                updateReachingDefB1(jj);
                updateReachingDefB2();
            }
            updateReachingDefA(jj, isBPartiallyOverlapA);
        }
        else {
            //even if B does not overlap with A, B can affect the reaching defs of A
            //for example, B is a def of "v0", A is "v1"
            //  B can kill some reaching defs of A or affect the accessType of a reaching def
            updateReachingDefA(jj, OVERLAP_NO); //update reachingDefs of A
        }
    }//for each variable A in infoBasicBlock
    if(!isMerged) {
        /* create a new entry in infoBasicBlock */
        bb->infoBasicBlock[bb->num_regs].refCount = currentInfo.refCount;
        bb->infoBasicBlock[bb->num_regs].physicalType = typeB;
        if(hasAlias)
            bb->infoBasicBlock[bb->num_regs].accessType = updateAccess3(tmpType, currentInfo.accessType);
        else
            bb->infoBasicBlock[bb->num_regs].accessType = currentInfo.accessType;
#ifdef DEBUG_MERGE_ENTRY
        ALOGI("update accessType in case 3: VR %d %d accessType %d", regB, typeB, bb->infoBasicBlock[bb->num_regs].accessType);
#endif
        bb->infoBasicBlock[bb->num_regs].regNum = regB;
        for(k = 0; k < 8; k++)
            bb->infoBasicBlock[bb->num_regs].allocConstraints[k] = currentInfo.allocConstraints[k];
#ifdef DEBUG_MERGE_ENTRY
        ALOGI("isMerged is false, call updateDefUseTable");
#endif
        updateDefUseTable(); //use currentInfo to update defUseTable
        updateReachingDefB3(); //update currentInfo.reachingDefs if currentInfo defines variable B

        //copy from currentInfo.reachingDefs to bb->infoBasicBlock[bb->num_regs]
        bb->infoBasicBlock[bb->num_regs].num_reaching_defs = currentInfo.num_reaching_defs;
        for(k = 0; k < currentInfo.num_reaching_defs; k++)
            bb->infoBasicBlock[bb->num_regs].reachingDefs[k] = currentInfo.reachingDefs[k];
#ifdef DEBUG_MERGE_ENTRY
        ALOGI("try to update reaching defs for VR %d %d", regB, typeB);
        for(k = 0; k < bb->infoBasicBlock[bb->num_regs].num_reaching_defs; k++)
            ALOGI("reaching def %d @ %d for VR %d %d access %d", k, currentInfo.reachingDefs[k].offsetPC,
                  currentInfo.reachingDefs[k].regNum, currentInfo.reachingDefs[k].physicalType,
                  currentInfo.reachingDefs[k].accessType);
#endif
        bb->num_regs++;
        if(bb->num_regs >= MAX_REG_PER_BASICBLOCK) {
            ALOGE("too many VRs in a basic block");
            dvmAbort();
        }
        return -1;
    }
    return 0;
}

//!update reaching defs for infoBasicBlock[indexToA]

//!use currentInfo.reachingDefs to update reaching defs for variable A
void updateReachingDefA(int indexToA, OverlapCase isBPartiallyOverlapA) {
    if(indexToA < 0) return;
    int k, k2;
    OverlapCase isBPartiallyOverlapDef;
    if(currentInfo.accessType == REGACCESS_U) {
        return; //no update to reachingDefs of the VR
    }
    /* access in currentInfo is DU, D, or UD */
    if(isBPartiallyOverlapA == OVERLAP_B_COVER_A) {
        /* from this point on, the reachingDefs for variable A is a single def to currentInfo at offsetPC */
        currentBB->infoBasicBlock[indexToA].num_reaching_defs = 1;
        currentBB->infoBasicBlock[indexToA].reachingDefs[0].offsetPC = offsetPC;
        currentBB->infoBasicBlock[indexToA].reachingDefs[0].regNum = currentInfo.regNum;
        currentBB->infoBasicBlock[indexToA].reachingDefs[0].physicalType = currentInfo.physicalType;
        currentBB->infoBasicBlock[indexToA].reachingDefs[0].accessType = REGACCESS_D;
#ifdef DEBUG_REACHING_DEF
        ALOGI("single reaching def @ %d for VR %d %d", offsetPC, currentInfo.regNum, currentInfo.physicalType);
#endif
        return;
    }
    /* update reachingDefs for variable A to get rid of dead defs */
    /* Bug fix: it is possible that more than one reaching defs need to be removed
                after one reaching def is removed, num_reaching_defs--, but k should not change
    */
    for(k = 0; k < currentBB->infoBasicBlock[indexToA].num_reaching_defs; ) {
        /* remove one reaching def in one interation of the loop */
        //check overlapping between def & B
        isBPartiallyOverlapDef = getBPartiallyOverlapA(currentInfo.regNum, currentInfo.physicalType,
                                                       currentBB->infoBasicBlock[indexToA].reachingDefs[k].regNum,
                                                       currentBB->infoBasicBlock[indexToA].reachingDefs[k].physicalType);
#ifdef DEBUG_REACHING_DEF
        ALOGI("DEBUG B %d %d def %d %d %d", currentInfo.regNum, currentInfo.physicalType,
              currentBB->infoBasicBlock[indexToA].reachingDefs[k].regNum,
              currentBB->infoBasicBlock[indexToA].reachingDefs[k].physicalType,
              currentBB->infoBasicBlock[indexToA].reachingDefs[k].accessType);
#endif
        /* cases where one def nees to be removed:
           if B fully covers def, def is removed
           if B overlaps high half of def & def's accessType is H, def is removed
           if B overlaps low half of def & def's accessType is L, def is removed
        */
        if((isBPartiallyOverlapDef == OVERLAP_B_COVER_HIGH_OF_A &&
            currentBB->infoBasicBlock[indexToA].reachingDefs[k].accessType == REGACCESS_H) ||
           (isBPartiallyOverlapDef == OVERLAP_B_COVER_LOW_OF_A &&
            currentBB->infoBasicBlock[indexToA].reachingDefs[k].accessType == REGACCESS_L) ||
           isBPartiallyOverlapDef == OVERLAP_B_COVER_A
           ) { //remove def
            //shift from k+1 to end
            for(k2 = k+1; k2 < currentBB->infoBasicBlock[indexToA].num_reaching_defs; k2++)
                currentBB->infoBasicBlock[indexToA].reachingDefs[k2-1] = currentBB->infoBasicBlock[indexToA].reachingDefs[k2];
            currentBB->infoBasicBlock[indexToA].num_reaching_defs--;
        }
        /*
           if B overlaps high half of def & def's accessType is not H --> update accessType of def
        */
        else if(isBPartiallyOverlapDef == OVERLAP_B_COVER_HIGH_OF_A &&
                currentBB->infoBasicBlock[indexToA].reachingDefs[k].accessType != REGACCESS_H) {
            //low half is still valid
            if(getRegSize(currentBB->infoBasicBlock[indexToA].reachingDefs[k].physicalType) == OpndSize_32)
                currentBB->infoBasicBlock[indexToA].reachingDefs[k].accessType = REGACCESS_D;
            else
                currentBB->infoBasicBlock[indexToA].reachingDefs[k].accessType = REGACCESS_L;
#ifdef DEBUG_REACHING_DEF
            ALOGI("DEBUG: set accessType of def to L");
#endif
            k++;
        }
        /*
           if B overlaps low half of def & def's accessType is not L --> update accessType of def
        */
        else if(isBPartiallyOverlapDef == OVERLAP_B_COVER_LOW_OF_A &&
                currentBB->infoBasicBlock[indexToA].reachingDefs[k].accessType != REGACCESS_L) {
            //high half of def is still valid
            currentBB->infoBasicBlock[indexToA].reachingDefs[k].accessType = REGACCESS_H;
#ifdef DEBUG_REACHING_DEF
            ALOGI("DEBUG: set accessType of def to H");
#endif
            k++;
        }
        else {
            k++;
        }
    }//for k
    if(isBPartiallyOverlapA != OVERLAP_NO) {
        //insert the def to variable @ currentInfo
        k = currentBB->infoBasicBlock[indexToA].num_reaching_defs;
        if(k >= 3) {
          ALOGE("more than 3 reaching defs");
        }
        currentBB->infoBasicBlock[indexToA].reachingDefs[k].offsetPC = offsetPC;
        currentBB->infoBasicBlock[indexToA].reachingDefs[k].regNum = currentInfo.regNum;
        currentBB->infoBasicBlock[indexToA].reachingDefs[k].physicalType = currentInfo.physicalType;
        currentBB->infoBasicBlock[indexToA].reachingDefs[k].accessType = REGACCESS_D;
        currentBB->infoBasicBlock[indexToA].num_reaching_defs++;
    }
#ifdef DEBUG_REACHING_DEF2
    ALOGI("IN updateReachingDefA for VR %d %d", currentBB->infoBasicBlock[indexToA].regNum,
          currentBB->infoBasicBlock[indexToA].physicalType);
    for(k = 0; k < currentBB->infoBasicBlock[indexToA].num_reaching_defs; k++)
        ALOGI("reaching def %d @ %d for VR %d %d access %d", k,
              currentBB->infoBasicBlock[indexToA].reachingDefs[k].offsetPC,
              currentBB->infoBasicBlock[indexToA].reachingDefs[k].regNum,
              currentBB->infoBasicBlock[indexToA].reachingDefs[k].physicalType,
              currentBB->infoBasicBlock[indexToA].reachingDefs[k].accessType);
#endif
}

/** Given a variable B @currentInfo,
    updates its reaching defs by checking reaching defs of variable A @currentBB->infoBasicBlock[indexToA]
    The result is stored in tmpInfo.reachingDefs
*/
void updateReachingDefB1(int indexToA) {
    if(indexToA < 0) return;
    int k;
    tmpInfo.num_reaching_defs = 0;
    for(k = 0; k < currentBB->infoBasicBlock[indexToA].num_reaching_defs; k++) {
        /* go through reachingDefs of variable A @currentBB->infoBasicBlock[indexToA]
           for each def, check whether it overlaps with variable B @currentInfo
               if the def overlaps with variable B, insert it to tmpInfo.reachingDefs
        */
        OverlapCase isDefPartiallyOverlapB = getAPartiallyOverlapB(
                                                 currentBB->infoBasicBlock[indexToA].reachingDefs[k].regNum,
                                                 currentBB->infoBasicBlock[indexToA].reachingDefs[k].physicalType,
                                                 currentInfo.regNum, currentInfo.physicalType
                                                 );
        bool insert1 = false; //whether to insert the def to tmpInfo.reachingDefs
        if(isDefPartiallyOverlapB == OVERLAP_ALIGN ||
           isDefPartiallyOverlapB == OVERLAP_A_IS_LOW_OF_B ||
           isDefPartiallyOverlapB == OVERLAP_A_IS_HIGH_OF_B) {
            /* B aligns with def */
            /* def is low half of B, def is high half of B
               in these two cases, def is 32 bits */
            insert1 = true;
        }
        RegAccessType deftype = currentBB->infoBasicBlock[indexToA].reachingDefs[k].accessType;
        if(isDefPartiallyOverlapB == OVERLAP_B_IS_LOW_OF_A ||
           isDefPartiallyOverlapB == OVERLAP_LOW_OF_A_IS_HIGH_OF_B) {
            /* B is the low half of def */
            /* the low half of def is the high half of B */
            if(deftype != REGACCESS_H) insert1 = true;
        }
        if(isDefPartiallyOverlapB == OVERLAP_B_IS_HIGH_OF_A ||
           isDefPartiallyOverlapB == OVERLAP_HIGH_OF_A_IS_LOW_OF_B) {
            /* B is the high half of def */
            /* the high half of def is the low half of B */
            if(deftype != REGACCESS_L) insert1 = true;
        }
        if(insert1) {
            if(tmpInfo.num_reaching_defs >= 3) {
                ALOGE("more than 3 reaching defs for tmpInfo");
            }
            tmpInfo.reachingDefs[tmpInfo.num_reaching_defs] = currentBB->infoBasicBlock[indexToA].reachingDefs[k];
            tmpInfo.num_reaching_defs++;
#ifdef DEBUG_REACHING_DEF2
            ALOGI("insert from entry %d %d: index %d", currentBB->infoBasicBlock[indexToA].regNum,
                  currentBB->infoBasicBlock[indexToA].physicalType, k);
#endif
        }
    }
}

/** update currentInfo.reachingDefs by merging currentInfo.reachingDefs with tmpInfo.reachingDefs
*/
void updateReachingDefB2() {
    int k, k2;
    for(k2 = 0; k2 < tmpInfo.num_reaching_defs; k2++ ) {
        bool merged = false;
        for(k = 0; k < currentInfo.num_reaching_defs; k++) {
            /* check whether it is the same def, if yes, do nothing */
            if(currentInfo.reachingDefs[k].regNum == tmpInfo.reachingDefs[k2].regNum &&
               currentInfo.reachingDefs[k].physicalType == tmpInfo.reachingDefs[k2].physicalType) {
                merged = true;
                if(currentInfo.reachingDefs[k].offsetPC != tmpInfo.reachingDefs[k2].offsetPC) {
                    ALOGE("defs on the same VR %d %d with different offsetPC %d vs %d",
                          currentInfo.reachingDefs[k].regNum, currentInfo.reachingDefs[k].physicalType,
                          currentInfo.reachingDefs[k].offsetPC, tmpInfo.reachingDefs[k2].offsetPC);
                }
                if(currentInfo.reachingDefs[k].accessType != tmpInfo.reachingDefs[k2].accessType)
                    ALOGE("defs on the same VR %d %d with different accessType",
                          currentInfo.reachingDefs[k].regNum, currentInfo.reachingDefs[k].physicalType);
                break;
            }
        }
        if(!merged) {
            if(currentInfo.num_reaching_defs >= 3) {
               ALOGE("more than 3 reaching defs for currentInfo");
            }
            currentInfo.reachingDefs[currentInfo.num_reaching_defs] = tmpInfo.reachingDefs[k2];
            currentInfo.num_reaching_defs++;
        }
    }
}

//!update currentInfo.reachingDefs with currentInfo if variable is defined in currentInfo

//!
void updateReachingDefB3() {
    if(currentInfo.accessType == REGACCESS_U) {
        return; //no need to update currentInfo.reachingDefs
    }
    currentInfo.num_reaching_defs = 1;
    currentInfo.reachingDefs[0].regNum = currentInfo.regNum;
    currentInfo.reachingDefs[0].physicalType = currentInfo.physicalType;
    currentInfo.reachingDefs[0].offsetPC = offsetPC;
    currentInfo.reachingDefs[0].accessType = REGACCESS_D;
}

/** update defUseTable by checking currentInfo
*/
void updateDefUseTable() {
    /* no access */
    if(currentInfo.accessType == REGACCESS_N) return;
    /* define then use, or define only */
    if(currentInfo.accessType == REGACCESS_DU || currentInfo.accessType == REGACCESS_D) {
        /* insert a definition at offsetPC to variable @ currentInfo */
        DefUsePair* ptr = insertADef(offsetPC, currentInfo.regNum, currentInfo.physicalType, REGACCESS_D);
        if(currentInfo.accessType != REGACCESS_D) {
             /* if access is define then use, insert a use at offsetPC */
            insertAUse(ptr, offsetPC, currentInfo.regNum, currentInfo.physicalType);
        }
        return;
    }
    /* use only or use then define
       check the reaching defs for the usage */
    int k;
    bool isLCovered = false, isHCovered = false, isDCovered = false;
    for(k = 0; k < currentInfo.num_reaching_defs; k++) {
        /* insert a def currentInfo.reachingDefs[k] and a use of variable at offsetPC */
        RegAccessType useType = insertDefUsePair(k);
        if(useType == REGACCESS_D) isDCovered = true;
        if(useType == REGACCESS_L) isLCovered = true;
        if(useType == REGACCESS_H) isHCovered = true;
    }
    OpndSize useSize = getRegSize(currentInfo.physicalType);
    if((!isDCovered) && (!isLCovered)) {
        /* the low half of variable is not defined in the basic block
           so insert a def to the low half at START of the basic block */
        insertDefUsePair(-1);
    }
    if(useSize == OpndSize_64 && (!isDCovered) && (!isHCovered)) {
        /* the high half of variable is not defined in the basic block
           so insert a def to the high half at START of the basic block */
        insertDefUsePair(-2);
    }
    if(currentInfo.accessType == REGACCESS_UD) {
        /* insert a def at offsetPC to variable @ currentInfo */
        insertADef(offsetPC, currentInfo.regNum, currentInfo.physicalType, REGACCESS_D);
        return;
    }
}

//! insert a use at offsetPC of given variable at end of DefUsePair

//!
RegAccessType insertAUse(DefUsePair* ptr, int offsetPC, int regNum, LowOpndRegType physicalType) {
    DefOrUseLink* tLink = (DefOrUseLink*)malloc(sizeof(DefOrUseLink));
    if(tLink == NULL) {
        ALOGE("Memory allocation failed");
        return REGACCESS_UNKNOWN;
    }
    tLink->offsetPC = offsetPC;
    tLink->regNum = regNum;
    tLink->physicalType = physicalType;
    tLink->next = NULL;
    if(ptr->useTail != NULL)
        ptr->useTail->next = tLink;
    ptr->useTail = tLink;
    if(ptr->uses == NULL)
        ptr->uses = tLink;
    ptr->num_uses++;

    //check whether the def is partially overlapping with the variable
    OverlapCase isDefPartiallyOverlapB = getBPartiallyOverlapA(ptr->def.regNum,
                                                       ptr->def.physicalType,
                                                       regNum, physicalType);
    RegAccessType useType = setAccessTypeOfUse(isDefPartiallyOverlapB, ptr->def.accessType);
    tLink->accessType = useType;
    return useType;
}

//! insert a def to currentBB->defUseTable

//! update currentBB->defUseTail if necessary
DefUsePair* insertADef(int offsetPC, int regNum, LowOpndRegType pType, RegAccessType rType) {
    DefUsePair* ptr = (DefUsePair*)malloc(sizeof(DefUsePair));
    if(ptr == NULL) {
        ALOGE("Memory allocation failed");
        return NULL;
    }
    ptr->next = NULL;
    ptr->def.offsetPC = offsetPC;
    ptr->def.regNum = regNum;
    ptr->def.physicalType = pType;
    ptr->def.accessType = rType;
    ptr->num_uses = 0;
    ptr->useTail = NULL;
    ptr->uses = NULL;
    if(currentBB->defUseTail != NULL) {
        currentBB->defUseTail->next = ptr;
    }
    currentBB->defUseTail = ptr;
    if(currentBB->defUseTable == NULL)
        currentBB->defUseTable = ptr;
    currentBB->num_defs++;
#ifdef DEBUG_REACHING_DEF
    ALOGI("insert a def at %d to defUseTable for VR %d %d", offsetPC,
          regNum, pType);
#endif
    return ptr;
}

/** insert a def to defUseTable, then insert a use of variable @ currentInfo
    if reachingDefIndex >= 0, the def is currentInfo.reachingDefs[index]
    if reachingDefIndex is -1, the low half is defined at START of the basic block
    if reachingDefIndex is -2, the high half is defined at START of the basic block
*/
RegAccessType insertDefUsePair(int reachingDefIndex) {
    int k = reachingDefIndex;
    DefUsePair* tableIndex = NULL;
    DefOrUse theDef;
    theDef.regNum = 0;
    if(k < 0) {
        /* def at start of the basic blcok */
        theDef.offsetPC = PC_FOR_START_OF_BB;
        theDef.accessType = REGACCESS_D;
        if(k == -1) //low half of variable
            theDef.regNum = currentInfo.regNum;
        if(k == -2) //high half of variable
            theDef.regNum = currentInfo.regNum+1;
        theDef.physicalType = LowOpndRegType_gp;
    }
    else {
        theDef = currentInfo.reachingDefs[k];
    }
    tableIndex = searchDefUseTable(theDef.offsetPC, theDef.regNum, theDef.physicalType);
    if(tableIndex == NULL) //insert an entry
        tableIndex = insertADef(theDef.offsetPC, theDef.regNum, theDef.physicalType, theDef.accessType);
    else
        tableIndex->def.accessType = theDef.accessType;
    RegAccessType useType = insertAUse(tableIndex, offsetPC, currentInfo.regNum, currentInfo.physicalType);
    return useType;
}

//! insert a XFER_MEM_TO_XMM to currentBB->xferPoints

//!
void insertLoadXfer(int offset, int regNum, LowOpndRegType pType) {
    //check whether it is already in currentBB->xferPoints
    int k;
    for(k = 0; k < currentBB->num_xfer_points; k++) {
        if(currentBB->xferPoints[k].xtype == XFER_MEM_TO_XMM &&
           currentBB->xferPoints[k].offsetPC == offset &&
           currentBB->xferPoints[k].regNum == regNum &&
           currentBB->xferPoints[k].physicalType == pType)
            return;
    }
    currentBB->xferPoints[currentBB->num_xfer_points].xtype = XFER_MEM_TO_XMM;
    currentBB->xferPoints[currentBB->num_xfer_points].regNum = regNum;
    currentBB->xferPoints[currentBB->num_xfer_points].offsetPC = offset;
    currentBB->xferPoints[currentBB->num_xfer_points].physicalType = pType;
#ifdef DEBUG_XFER_POINTS
    ALOGI("insert to xferPoints %d: XFER_MEM_TO_XMM of VR %d %d at %d", currentBB->num_xfer_points, regNum, pType, offset);
#endif
    currentBB->num_xfer_points++;
    if(currentBB->num_xfer_points >= MAX_XFER_PER_BB) {
        ALOGE("too many xfer points");
        dvmAbort();
    }
}

/** update defUseTable by assuming a fake usage at END of a basic block for variable @ currentInfo
    create a fake usage at end of a basic block for variable B (currentInfo.physicalType, currentInfo.regNum)
    get reaching def info for variable B and store the info in currentInfo.reachingDefs
        for each virtual register (variable A) accessed in the basic block
            update reaching defs of B by checking reaching defs of variable A
    update defUseTable
*/
int fakeUsageAtEndOfBB(BasicBlock_O1* bb) {
    currentInfo.accessType = REGACCESS_U;
    LowOpndRegType typeB = currentInfo.physicalType;
    int regB = currentInfo.regNum;
    int jj, k;
    currentInfo.num_reaching_defs = 0;
    for(jj = 0; jj < bb->num_regs; jj++) {
        int regA = bb->infoBasicBlock[jj].regNum;
        LowOpndRegType typeA = bb->infoBasicBlock[jj].physicalType;
        OverlapCase isBPartiallyOverlapA = getBPartiallyOverlapA(regB, typeB, regA, typeA);
        if(regA == regB && typeA == typeB) {
            /* copy reachingDefs from variable A */
            currentInfo.num_reaching_defs = bb->infoBasicBlock[jj].num_reaching_defs;
            for(k = 0; k < currentInfo.num_reaching_defs; k++)
                currentInfo.reachingDefs[k] = bb->infoBasicBlock[jj].reachingDefs[k];
            break;
        }
        else if(isBPartiallyOverlapA != OVERLAP_NO) {
            /* B overlaps with A */
            /* update reaching defs of variable B by checking reaching defs of bb->infoBasicBlock[jj] */
            updateReachingDefB1(jj);
            updateReachingDefB2(); //merge currentInfo with tmpInfo
        }
    }
    /* update defUseTable by checking currentInfo */
    updateDefUseTable();
    return 0;
}

/** update xferPoints of currentBB
    Traverse currentBB->defUseTable
*/
int updateXferPoints() {
    int k = 0;
    currentBB->num_xfer_points = 0;
    DefUsePair* ptr = currentBB->defUseTable;
    DefOrUseLink* ptrUse = NULL;
    /* traverse the def use chain of the basic block */
    while(ptr != NULL) {
        LowOpndRegType defType = ptr->def.physicalType;
        //if definition is for a variable of 32 bits
        if(getRegSize(defType) == OpndSize_32) {
            /* check usages of the definition, whether it reaches a GPR, a XMM, a FS, or a SS */
            bool hasGpUsage = false;
            bool hasGpUsage2 = false; //not a fake usage
            bool hasXmmUsage = false;
            bool hasFSUsage = false;
            bool hasSSUsage = false;
            ptrUse = ptr->uses;
            while(ptrUse != NULL) {
                if(ptrUse->physicalType == LowOpndRegType_gp) {
                    hasGpUsage = true;
                    if(ptrUse->offsetPC != PC_FOR_END_OF_BB)
                        hasGpUsage2 = true;
                }
                if(ptrUse->physicalType == LowOpndRegType_ss) hasSSUsage = true;
                if(ptrUse->physicalType == LowOpndRegType_fs ||
                   ptrUse->physicalType == LowOpndRegType_fs_s)
                    hasFSUsage = true;
                if(ptrUse->physicalType == LowOpndRegType_xmm) {
                    hasXmmUsage = true;
                }
                if(ptrUse->physicalType == LowOpndRegType_xmm ||
                   ptrUse->physicalType == LowOpndRegType_ss) {
                    /* if a 32-bit definition reaches a xmm usage or a SS usage,
                       insert a XFER_MEM_TO_XMM */
                    insertLoadXfer(ptrUse->offsetPC,
                                   ptrUse->regNum, LowOpndRegType_xmm);
                }
                ptrUse = ptrUse->next;
            }
            if(((hasXmmUsage || hasFSUsage || hasSSUsage) && defType == LowOpndRegType_gp) ||
               (hasGpUsage && defType == LowOpndRegType_fs) ||
               (defType == LowOpndRegType_ss && (hasGpUsage || hasXmmUsage || hasFSUsage))) {
                /* insert a transfer if def is on a GPR, usage is on a XMM, FS or SS
                                     if def is on a FS, usage is on a GPR
                                     if def is on a SS, usage is on a GPR, XMM or FS
                   transfer type is XFER_DEF_TO_GP_MEM if a real GPR usage exisits
                   transfer type is XFER_DEF_TO_GP otherwise*/
                currentBB->xferPoints[currentBB->num_xfer_points].offsetPC = ptr->def.offsetPC;
                currentBB->xferPoints[currentBB->num_xfer_points].regNum = ptr->def.regNum;
                currentBB->xferPoints[currentBB->num_xfer_points].physicalType = ptr->def.physicalType;
                if(hasGpUsage2) { //create an entry XFER_DEF_TO_GP_MEM
                    currentBB->xferPoints[currentBB->num_xfer_points].xtype = XFER_DEF_TO_GP_MEM;
                }
                else { //create an entry XFER_DEF_TO_MEM
                    currentBB->xferPoints[currentBB->num_xfer_points].xtype = XFER_DEF_TO_MEM;
                }
                currentBB->xferPoints[currentBB->num_xfer_points].tableIndex = k;
#ifdef DEBUG_XFER_POINTS
                ALOGI("insert XFER %d at def %d: V%d %d", currentBB->num_xfer_points, ptr->def.offsetPC, ptr->def.regNum, defType);
#endif
                currentBB->num_xfer_points++;
                if(currentBB->num_xfer_points >= MAX_XFER_PER_BB) {
                    ALOGE("too many xfer points");
                    dvmAbort();
                }
            }
        }
        else { /* def is on 64 bits */
            bool hasGpUsageOfL = false; //exist a GPR usage of the low half
            bool hasGpUsageOfH = false; //exist a GPR usage of the high half
            bool hasGpUsageOfL2 = false;
            bool hasGpUsageOfH2 = false;
            bool hasMisaligned = false;
            bool hasAligned = false;
            bool hasFSUsage = false;
            bool hasSSUsage = false;
            ptrUse = ptr->uses;
            while(ptrUse != NULL) {
                if(ptrUse->physicalType == LowOpndRegType_gp &&
                   ptrUse->regNum == ptr->def.regNum) {
                    hasGpUsageOfL = true;
                    if(ptrUse->offsetPC != PC_FOR_END_OF_BB)
                        hasGpUsageOfL2 = true;
                }
                if(ptrUse->physicalType == LowOpndRegType_gp &&
                   ptrUse->regNum == ptr->def.regNum + 1) {
                    hasGpUsageOfH = true;
                    if(ptrUse->offsetPC != PC_FOR_END_OF_BB)
                        hasGpUsageOfH2 = true;
                }
                if(ptrUse->physicalType == LowOpndRegType_xmm &&
                   ptrUse->regNum == ptr->def.regNum) {
                    hasAligned = true;
                    /* if def is on FS and use is on XMM, insert a XFER_MEM_TO_XMM */
                    if(defType == LowOpndRegType_fs)
                        insertLoadXfer(ptrUse->offsetPC,
                                       ptrUse->regNum, LowOpndRegType_xmm);
                }
                if(ptrUse->physicalType == LowOpndRegType_fs ||
                   ptrUse->physicalType == LowOpndRegType_fs_s)
                    hasFSUsage = true;
                if(ptrUse->physicalType == LowOpndRegType_xmm &&
                   ptrUse->regNum != ptr->def.regNum) {
                    hasMisaligned = true;
                    /* if use is on XMM and use and def are misaligned, insert a XFER_MEM_TO_XMM */
                    insertLoadXfer(ptrUse->offsetPC,
                                   ptrUse->regNum, LowOpndRegType_xmm);
                }
                if(ptrUse->physicalType == LowOpndRegType_ss) {
                    hasSSUsage = true;
                    /* if use is on SS, insert a XFER_MEM_TO_XMM */
                    insertLoadXfer(ptrUse->offsetPC,
                                   ptrUse->regNum, LowOpndRegType_ss);
                }
                ptrUse = ptrUse->next;
            }
            if(defType == LowOpndRegType_fs && !hasGpUsageOfL && !hasGpUsageOfH) {
                ptr = ptr->next;
                continue;
            }
            if(defType == LowOpndRegType_xmm && !hasFSUsage &&
               !hasGpUsageOfL && !hasGpUsageOfH && !hasMisaligned && !hasSSUsage) {
                ptr = ptr->next;
                continue;
            }
            /* insert a XFER_DEF_IS_XMM */
            currentBB->xferPoints[currentBB->num_xfer_points].regNum = ptr->def.regNum;
            currentBB->xferPoints[currentBB->num_xfer_points].offsetPC = ptr->def.offsetPC;
            currentBB->xferPoints[currentBB->num_xfer_points].physicalType = ptr->def.physicalType;
            currentBB->xferPoints[currentBB->num_xfer_points].xtype = XFER_DEF_IS_XMM;
            currentBB->xferPoints[currentBB->num_xfer_points].vr_gpl = -1;
            currentBB->xferPoints[currentBB->num_xfer_points].vr_gph = -1;
            if(hasGpUsageOfL2) currentBB->xferPoints[currentBB->num_xfer_points].vr_gpl = ptr->def.regNum;
            if(hasGpUsageOfH2) currentBB->xferPoints[currentBB->num_xfer_points].vr_gph = ptr->def.regNum+1;
            currentBB->xferPoints[currentBB->num_xfer_points].dumpToMem = true;
            currentBB->xferPoints[currentBB->num_xfer_points].dumpToXmm = false; //not used in updateVirtualReg
            if(hasAligned) currentBB->xferPoints[currentBB->num_xfer_points].dumpToXmm = true;
            currentBB->xferPoints[currentBB->num_xfer_points].tableIndex = k;
#ifdef DEBUG_XFER_POINTS
            ALOGI("insert XFER %d at def %d: V%d %d", currentBB->num_xfer_points, ptr->def.offsetPC, ptr->def.regNum, defType);
#endif
            currentBB->num_xfer_points++;
            if(currentBB->num_xfer_points >= MAX_XFER_PER_BB) {
                ALOGE("too many xfer points");
                dvmAbort();
            }
        }
        ptr = ptr->next;
    } //while ptr
#ifdef DEBUG_XFER_POINTS
    ALOGI("XFER points for current basic block ------");
    for(k = 0; k < currentBB->num_xfer_points; k++) {
        ALOGI("  at offset %x, VR %d %d: type %d, vr_gpl %d, vr_gph %d, dumpToMem %d, dumpToXmm %d",
              currentBB->xferPoints[k].offsetPC, currentBB->xferPoints[k].regNum,
              currentBB->xferPoints[k].physicalType, currentBB->xferPoints[k].xtype,
              currentBB->xferPoints[k].vr_gpl, currentBB->xferPoints[k].vr_gph,
              currentBB->xferPoints[k].dumpToMem, currentBB->xferPoints[k].dumpToXmm);
    }
#endif
    return -1;
}

//! update memVRTable[].ranges by browsing the defUseTable

//! each virtual register has a list of live ranges, and each live range has a list of PCs that access the VR
void updateLiveTable() {
    DefUsePair* ptr = currentBB->defUseTable;
    while(ptr != NULL) {
        bool updateUse = false;
        if(ptr->num_uses == 0) {
            ptr->num_uses = 1;
            ptr->uses = (DefOrUseLink*)malloc(sizeof(DefOrUseLink));
            if(ptr->uses == NULL) {
                ALOGE("Memory allocation failed");
                return;
            }
            ptr->uses->accessType = REGACCESS_D;
            ptr->uses->regNum = ptr->def.regNum;
            ptr->uses->offsetPC = ptr->def.offsetPC;
            ptr->uses->physicalType = ptr->def.physicalType;
            ptr->uses->next = NULL;
            ptr->useTail = ptr->uses;
            updateUse = true;
        }
        DefOrUseLink* ptrUse = ptr->uses;
        while(ptrUse != NULL) {
            RegAccessType useType = ptrUse->accessType;
            if(useType == REGACCESS_L || useType == REGACCESS_D) {
                int indexL = searchMemTable(ptrUse->regNum);
                if(indexL >= 0)
                    mergeLiveRange(indexL, ptr->def.offsetPC,
                                   ptrUse->offsetPC); //tableIndex, start PC, end PC
            }
            if(getRegSize(ptrUse->physicalType) == OpndSize_64 &&
               (useType == REGACCESS_H || useType == REGACCESS_D)) {
                int indexH = searchMemTable(ptrUse->regNum+1);
                if(indexH >= 0)
                    mergeLiveRange(indexH, ptr->def.offsetPC,
                                   ptrUse->offsetPC);
            }
            ptrUse = ptrUse->next;
        }//while ptrUse
        if(updateUse) {
            ptr->num_uses = 0;
            free(ptr->uses);
            ptr->uses = NULL;
            ptr->useTail = NULL;
        }
        ptr = ptr->next;
    }//while ptr
#ifdef DEBUG_LIVE_RANGE
    ALOGI("LIVE TABLE");
    for(int k = 0; k < num_memory_vr; k++) {
        ALOGI("VR %d live ", memVRTable[k].regNum);
        LiveRange* ptr = memVRTable[k].ranges;
        while(ptr != NULL) {
            ALOGI("[%x %x] (", ptr->start, ptr->end);
            for(int k3 = 0; k3 < ptr->num_access; k3++)
                ALOGI("%x ", ptr->accessPC[k3]);
            ALOGI(") ");
            ptr = ptr->next;
        }
        ALOGI("");
    }
#endif
}

//!add a live range [rangeStart, rangeEnd] to ranges of memVRTable, merge to existing live ranges if necessary

//!ranges are in increasing order of startPC
void mergeLiveRange(int tableIndex, int rangeStart, int rangeEnd) {
    if(rangeStart == PC_FOR_START_OF_BB) rangeStart = currentBB->pc_start;
    if(rangeEnd == PC_FOR_END_OF_BB) rangeEnd = currentBB->pc_end;
#ifdef DEBUG_LIVE_RANGE
    ALOGI("LIVERANGE call mergeLiveRange on tableIndex %d with [%x %x]", tableIndex, rangeStart, rangeEnd);
#endif
    int startIndex = -1, endIndex = -1;
    bool startBeforeRange = false, endBeforeRange = false; //before the index or in the range
    bool startDone = false, endDone = false;
    LiveRange* ptr = memVRTable[tableIndex].ranges;
    LiveRange* ptrStart = NULL;
    LiveRange* ptrStart_prev = NULL;
    LiveRange* ptrEnd = NULL;
    LiveRange* ptrEnd_prev = NULL;
    int k = 0;
    while(ptr != NULL) {
        if(!startDone) {
            if(ptr->start <= rangeStart &&
               ptr->end >= rangeStart) {
                startIndex = k;
                ptrStart = ptr;
                startBeforeRange = false;
                startDone = true;
            }
            else if(ptr->start > rangeStart) {
                startIndex = k;
                ptrStart = ptr;
                startBeforeRange = true;
                startDone = true;
            }
        }
        if(!startDone) ptrStart_prev = ptr;
        if(!endDone) {
            if(ptr->start <= rangeEnd &&
               ptr->end >= rangeEnd) {
                endIndex = k;
                ptrEnd = ptr;
                endBeforeRange = false;
                endDone = true;
            }
            else if(ptr->start > rangeEnd) {
                endIndex = k;
                ptrEnd = ptr;
                endBeforeRange = true;
                endDone = true;
            }
        }
        if(!endDone) ptrEnd_prev = ptr;
        ptr = ptr->next;
        k++;
    } //while
    if(!startDone) { //both can be NULL
        startIndex = memVRTable[tableIndex].num_ranges;
        ptrStart = NULL; //ptrStart_prev should be the last live range
        startBeforeRange = true;
    }
    //if endDone, ptrEnd is not NULL, ptrEnd_prev can be NULL
    if(!endDone) { //both can be NULL
        endIndex = memVRTable[tableIndex].num_ranges;
        ptrEnd = NULL;
        endBeforeRange = true;
    }
    if(startIndex == endIndex && startBeforeRange && endBeforeRange) { //insert at startIndex
        //3 cases depending on BeforeRange when startIndex == endIndex
        //insert only if both true
        //merge otherwise
        /////////// insert before ptrStart
        LiveRange* currRange = (LiveRange *)malloc(sizeof(LiveRange));
        if(ptrStart_prev == NULL) {
            currRange->next = memVRTable[tableIndex].ranges;
            memVRTable[tableIndex].ranges = currRange;
        } else {
            currRange->next = ptrStart_prev->next;
            ptrStart_prev->next = currRange;
        }
        currRange->start = rangeStart;
        currRange->end = rangeEnd;
        currRange->accessPC = (int *)malloc(sizeof(int) * NUM_ACCESS_IN_LIVERANGE);
        currRange->num_alloc = NUM_ACCESS_IN_LIVERANGE;
        if(rangeStart != rangeEnd) {
            currRange->num_access = 2;
            currRange->accessPC[0] = rangeStart;
            currRange->accessPC[1] = rangeEnd;
        } else {
            currRange->num_access = 1;
            currRange->accessPC[0] = rangeStart;
        }
        memVRTable[tableIndex].num_ranges++;
#ifdef DEBUG_LIVE_RANGE
        ALOGI("LIVERANGE insert one live range [%x %x] to tableIndex %d", rangeStart, rangeEnd, tableIndex);
#endif
        return;
    }
    if(!endBeforeRange) { //here ptrEnd is not NULL
        endIndex++; //next
        ptrEnd_prev = ptrEnd; //ptrEnd_prev is not NULL
        ptrEnd = ptrEnd->next; //ptrEnd can be NULL
    }
    if(endIndex < startIndex+1) ALOGE("mergeLiveRange endIndex %d startIndex %d", endIndex, startIndex);
    ///////// use ptrStart & ptrEnd_prev
    if(ptrStart == NULL || ptrEnd_prev == NULL) {
        ALOGE("mergeLiveRange ptr is NULL");
        return;
    }
    //endIndex > startIndex (merge the ranges between startIndex and endIndex-1)
    //update ptrStart
    if(ptrStart->start > rangeStart)
        ptrStart->start = rangeStart; //min of old start & rangeStart
    ptrStart->end = ptrEnd_prev->end; //max of old end & rangeEnd
    if(rangeEnd > ptrStart->end)
        ptrStart->end = rangeEnd;
#ifdef DEBUG_LIVE_RANGE
    ALOGI("LIVERANGE merge entries for tableIndex %d from %d to %d", tableIndex, startIndex+1, endIndex-1);
#endif
    if(ptrStart->num_access <= 0) ALOGE("mergeLiveRange number of access");
#ifdef DEBUG_LIVE_RANGE
    ALOGI("LIVERANGE tableIndex %d startIndex %d num_access %d (", tableIndex, startIndex, ptrStart->num_access);
    for(k = 0; k < ptrStart->num_access; k++)
        ALOGI("%x ", ptrStart->accessPC[k]);
    ALOGI(")");
#endif
    ///// go through pointers from ptrStart->next to ptrEnd
    //from startIndex+1 to endIndex-1
    ptr = ptrStart->next;
    while(ptr != NULL && ptr != ptrEnd) {
        int k2;
        for(k2 = 0; k2 < ptr->num_access; k2++) { //merge to startIndex
            insertAccess(tableIndex, ptrStart, ptr->accessPC[k2]);
        }//k2
        ptr = ptr->next;
    }
    insertAccess(tableIndex, ptrStart, rangeStart);
    insertAccess(tableIndex, ptrStart, rangeEnd);
    //remove startIndex+1 to endIndex-1
    if(startIndex+1 < endIndex) {
        ptr = ptrStart->next;
        while(ptr != NULL && ptr != ptrEnd) {
            LiveRange* tmpP = ptr->next;
            free(ptr->accessPC);
            free(ptr);
            ptr = tmpP;
        }
        ptrStart->next = ptrEnd;
    }
    memVRTable[tableIndex].num_ranges -= (endIndex - startIndex - 1);
#ifdef DEBUG_LIVE_RANGE
    ALOGI("num_ranges for VR %d: %d", memVRTable[tableIndex].regNum, memVRTable[tableIndex].num_ranges);
#endif
}
//! insert an access to a given live range, in order

//!
void insertAccess(int tableIndex, LiveRange* startP, int rangeStart) {
    int k3, k4;
#ifdef DEBUG_LIVE_RANGE
    ALOGI("LIVERANGE insertAccess %d %x", tableIndex, rangeStart);
#endif
    int insertIndex = -1;
    for(k3 = 0; k3 < startP->num_access; k3++) {
        if(startP->accessPC[k3] == rangeStart) {
            return;
        }
        if(startP->accessPC[k3] > rangeStart) {
            insertIndex = k3;
            break;
        }
    }

    //insert here
    k3 = insertIndex;
    if(insertIndex == -1) {
        k3 = startP->num_access;
    }
    if(startP->num_access == startP->num_alloc) {
        int currentAlloc = startP->num_alloc;
        startP->num_alloc += NUM_ACCESS_IN_LIVERANGE;
        int* tmpPtr = (int *)malloc(sizeof(int) * startP->num_alloc);
        for(k4 = 0; k4 < currentAlloc; k4++)
            tmpPtr[k4] = startP->accessPC[k4];
        free(startP->accessPC);
        startP->accessPC = tmpPtr;
    }
    //insert accessPC
    for(k4 = startP->num_access-1; k4 >= k3; k4--)
        startP->accessPC[k4+1] = startP->accessPC[k4];
    startP->accessPC[k3] = rangeStart;
#ifdef DEBUG_LIVE_RANGE
    ALOGI("LIVERANGE insert %x to tableIndex %d", rangeStart, tableIndex);
#endif
    startP->num_access++;
    return;
}

/////////////////////////////////////////////////////////////////////
bool isInMemory(int regNum, OpndSize size);
void setVRToMemory(int regNum, OpndSize size);
bool isVRLive(int vA);
int getSpillIndex(bool isGLUE, OpndSize size);
void clearVRToMemory(int regNum, OpndSize size);
void clearVRNullCheck(int regNum, OpndSize size);

inline int getSpillLocDisp(int offset) {
#ifdef SPILL_IN_THREAD
    return offset+offsetof(Thread, spillRegion);;
#else
    return offset+offEBP_spill;
#endif
}
#if 0
/* used if we keep self pointer in a physical register */
inline int getSpillLocReg(int offset) {
    return PhysicalReg_Glue;
}
#endif
#ifdef SPILL_IN_THREAD
inline void loadFromSpillRegion_with_self(OpndSize size, int reg_self, bool selfPhysical, int reg, int offset) {
    /* only 1 instruction is generated by move_mem_to_reg_noalloc */
    move_mem_to_reg_noalloc(size,
                            getSpillLocDisp(offset), reg_self, selfPhysical,
                            MemoryAccess_SPILL, offset,
                            reg, true);
}
inline void loadFromSpillRegion(OpndSize size, int reg, int offset) {
    get_self_pointer(C_SCRATCH_1, isScratchPhysical);
    int reg_self = registerAlloc(LowOpndRegType_scratch, C_SCRATCH_1, isScratchPhysical, false);
    /* only 1 instruction is generated by move_mem_to_reg_noalloc */
    move_mem_to_reg_noalloc(size,
                            getSpillLocDisp(offset), reg_self, true,
                            MemoryAccess_SPILL, offset,
                            reg, true);
}
inline void saveToSpillRegion_with_self(OpndSize size, int selfReg, bool selfPhysical, int reg, int offset) {
    move_reg_to_mem_noalloc(size,
                            reg, true,
                            getSpillLocDisp(offset), selfReg, selfPhysical,
                            MemoryAccess_SPILL, offset);
}
inline void saveToSpillRegion(OpndSize size, int reg, int offset) {
    get_self_pointer(C_SCRATCH_1, isScratchPhysical);
    int reg_self = registerAlloc(LowOpndRegType_scratch, C_SCRATCH_1, isScratchPhysical, false);
    move_reg_to_mem_noalloc(size,
                            reg, true,
                            getSpillLocDisp(offset), reg_self, true,
                            MemoryAccess_SPILL, offset);
}
#else
inline void loadFromSpillRegion(OpndSize size, int reg, int offset) {
    /* only 1 instruction is generated by move_mem_to_reg_noalloc */
    move_mem_to_reg_noalloc(size,
                            getSpillLocDisp(offset), PhysicalReg_EBP, true,
                            MemoryAccess_SPILL, offset,
                            reg, true);
}
inline void saveToSpillRegion(OpndSize size, int reg, int offset) {
    move_reg_to_mem_noalloc(size,
                            reg, true,
                            getSpillLocDisp(offset), PhysicalReg_EBP, true,
                            MemoryAccess_SPILL, offset);
}
#endif

//! dump an immediate to memory, set inMemory to true

//!
void dumpImmToMem(int vrNum, OpndSize size, int value) {
    if(isInMemory(vrNum, size)) {
#ifdef DEBUG_SPILL
        ALOGI("Skip dumpImmToMem vA %d size %d", vrNum, size);
#endif
        return;
    }
    set_VR_to_imm_noalloc(vrNum, size, value);
    setVRToMemory(vrNum, size);
}
//! dump content of a VR to memory, set inMemory to true

//!
void dumpToMem(int vrNum, LowOpndRegType type, int regAll) { //ss,gp,xmm
    if(isInMemory(vrNum, getRegSize(type))) {
#ifdef DEBUG_SPILL
        ALOGI("Skip dumpToMem vA %d type %d", vrNum, type);
#endif
        return;
    }
    if(type == LowOpndRegType_gp || type == LowOpndRegType_xmm)
        set_virtual_reg_noalloc(vrNum, getRegSize(type), regAll, true);
    if(type == LowOpndRegType_ss)
        move_ss_reg_to_mem_noalloc(regAll, true,
                                   4*vrNum, PhysicalReg_FP, true,
                                   MemoryAccess_VR, vrNum);
    setVRToMemory(vrNum, getRegSize(type));
}
//! dump part of a 64-bit VR to memory and update inMemory

//! isLow tells whether low half or high half is dumped
void dumpPartToMem(int reg /*xmm physical reg*/, int vA, bool isLow) {
    if(isLow) {
        if(isInMemory(vA, OpndSize_32)) {
#ifdef DEBUG_SPILL
            ALOGI("Skip dumpPartToMem isLow %d vA %d", isLow, vA);
#endif
            return;
        }
    }
    else {
        if(isInMemory(vA+1, OpndSize_32)) {
#ifdef DEBUG_SPILL
            ALOGI("Skip dumpPartToMem isLow %d vA %d", isLow, vA);
#endif
            return;
        }
    }
    if(isLow) {
        if(!isVRLive(vA)) return;
    }
    else {
        if(!isVRLive(vA+1)) return;
    }
    //move part to vA or vA+1
    if(isLow) {
        move_ss_reg_to_mem_noalloc(reg, true,
                                   4*vA, PhysicalReg_FP, true, MemoryAccess_VR, vA);
    } else {
        int k = getSpillIndex(false, OpndSize_64);
        //H, L in 4*k+4 & 4*k
#ifdef SPILL_IN_THREAD
        get_self_pointer(PhysicalReg_SCRATCH_1, isScratchPhysical);
        saveToSpillRegion_with_self(OpndSize_64, PhysicalReg_SCRATCH_1, isScratchPhysical, reg, 4*k);
        //update low 32 bits of xmm reg from 4*k+4
        move_ss_mem_to_reg(NULL,
                                   getSpillLocDisp(4*k+4), PhysicalReg_SCRATCH_1, isScratchPhysical,
                                   reg, true);
#else
        saveToSpillRegion(OpndSize_64, reg, 4*k);
        //update low 32 bits of xmm reg from 4*k+4
        move_ss_mem_to_reg_noalloc(
                                   getSpillLocDisp(4*k+4), PhysicalReg_EBP, true,
                                   MemoryAccess_SPILL, 4*k+4,
                                   reg, true);
#endif
        //move low 32 bits of xmm reg to vA+1
        move_ss_reg_to_mem_noalloc(reg, true, 4*(vA+1), PhysicalReg_FP, true, MemoryAccess_VR, vA+1);
    }
    if(isLow)
        setVRToMemory(vA, OpndSize_32);
    else
        setVRToMemory(vA+1, OpndSize_32);
}
void clearVRBoundCheck(int regNum, OpndSize size);
//! the content of a VR is no longer in memory or in physical register if the latest content of a VR is constant

//! clear nullCheckDone; if another VR is overlapped with the given VR, the content of that VR is no longer in physical register
void invalidateVRDueToConst(int reg, OpndSize size) {
    clearVRToMemory(reg, size); //memory content is out-dated
    clearVRNullCheck(reg, size);
    clearVRBoundCheck(reg, size);
    //check reg,gp reg,ss reg,xmm reg-1,xmm
    //if size is 64: check reg+1,gp|ss reg+1,xmm
    int index;
    //if VR is xmm, check whether we need to dump part of VR to memory
    index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_xmm, reg);
    if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
        ALOGI("INVALIDATE virtual reg %d type %d", reg, LowOpndRegType_xmm);
#endif
        if(size == OpndSize_32)
            dumpPartToMem(compileTable[index].physicalReg, reg, false); //dump high of xmm to memory
        compileTable[index].physicalReg = PhysicalReg_Null;
    }
    index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_xmm, reg-1);
    if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
        ALOGI("INVALIDATE virtual reg %d type %d", reg-1, LowOpndRegType_xmm);
#endif
        dumpPartToMem(compileTable[index].physicalReg, reg-1, true); //dump low of xmm to memory
        compileTable[index].physicalReg = PhysicalReg_Null;
    }
    index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_gp, reg);
    if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
        ALOGI("INVALIDATE virtual reg %d type %d", reg, LowOpndRegType_gp);
#endif
        compileTable[index].physicalReg = PhysicalReg_Null;
    }
    index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_ss, reg);
    if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
        ALOGI("INVALIDATE virtual reg %d type %d", reg, LowOpndRegType_ss);
#endif
        compileTable[index].physicalReg = PhysicalReg_Null;
    }
    if(size == OpndSize_64) {
        index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_xmm, reg+1);
        if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
            ALOGI("INVALIDATE virtual reg %d type %d", reg+1, LowOpndRegType_xmm);
#endif
            dumpPartToMem(compileTable[index].physicalReg, reg+1, false); //dump high of xmm to memory
            compileTable[index].physicalReg = PhysicalReg_Null;
        }
        index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_gp, reg+1);
        if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
            ALOGI("INVALIDATE virtual reg %d type %d", reg+1, LowOpndRegType_gp);
#endif
            compileTable[index].physicalReg = PhysicalReg_Null;
        }
        index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_ss, reg+1);
        if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
            ALOGI("INVALIDATE virtual reg %d type %d", reg+1, LowOpndRegType_ss);
#endif
            compileTable[index].physicalReg = PhysicalReg_Null;
        }
    }
}
//! check which physical registers hold out-dated content if there is a def

//! if another VR is overlapped with the given VR, the content of that VR is no longer in physical register
//! should we update inMemory?
void invalidateVR(int reg, LowOpndRegType pType) {
    //def at fs: content of xmm & gp & ss are out-dated (reg-1,xmm reg,xmm reg+1,xmm) (reg,gp|ss reg+1,gp|ss)
    //def at xmm: content of misaligned xmm & gp are out-dated (reg-1,xmm reg+1,xmm) (reg,gp|ss reg+1,gp|ss)
    //def at fs_s: content of xmm & gp are out-dated (reg-1,xmm reg,xmm) (reg,gp|ss)
    //def at gp:   content of xmm is out-dated (reg-1,xmm reg,xmm) (reg,ss)
    //def at ss:   content of xmm & gp are out-dated (reg-1,xmm reg,xmm) (reg,gp)
    int index;
    if(pType != LowOpndRegType_xmm) { //check xmm @reg
        index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_xmm, reg);
        if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
            ALOGI("INVALIDATE virtual reg %d type %d", reg, LowOpndRegType_xmm);
#endif
            if(getRegSize(pType) == OpndSize_32)
                dumpPartToMem(compileTable[index].physicalReg, reg, false); //dump high of xmm to memory
            compileTable[index].physicalReg = PhysicalReg_Null;
        }
    }
    //check misaligned xmm @ reg-1
    index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_xmm, reg-1);
    if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
        ALOGI("INVALIDATE virtual reg %d type %d", reg-1, LowOpndRegType_xmm);
#endif
        dumpPartToMem(compileTable[index].physicalReg, reg-1, true); //dump low of xmm to memory
        compileTable[index].physicalReg = PhysicalReg_Null;
    }
    //check misaligned xmm @ reg+1
    if(pType == LowOpndRegType_xmm || pType == LowOpndRegType_fs) {
        //check reg+1,xmm
        index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_xmm, reg+1);
        if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
            ALOGI("INVALIDATE virtual reg %d type %d", reg+1, LowOpndRegType_xmm);
#endif
            dumpPartToMem(compileTable[index].physicalReg, reg+1, false); //dump high of xmm to memory
            compileTable[index].physicalReg = PhysicalReg_Null;
        }
    }
    if(pType != LowOpndRegType_gp) {
        //check reg,gp
        index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_gp, reg);
        if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
            ALOGI("INVALIDATE virtual reg %d type %d", reg, LowOpndRegType_gp);
#endif
            compileTable[index].physicalReg = PhysicalReg_Null;
        }
    }
    if(pType == LowOpndRegType_xmm || pType == LowOpndRegType_fs) {
        //check reg+1,gp
        index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_gp, reg+1);
        if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
            ALOGI("INVALIDATE virtual reg %d type %d", reg+1, LowOpndRegType_gp);
#endif
            compileTable[index].physicalReg = PhysicalReg_Null;
        }
    }
    if(pType != LowOpndRegType_ss) {
        //check reg,ss
        index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_ss, reg);
        if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
            ALOGI("INVALIDATE virtual reg %d type %d", reg, LowOpndRegType_ss);
#endif
            compileTable[index].physicalReg = PhysicalReg_Null;
        }
    }
    if(pType == LowOpndRegType_xmm || pType == LowOpndRegType_fs) {
        //check reg+1,ss
        index = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_ss, reg+1);
        if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_INVALIDATE
            ALOGI("INVALIDATE virtual reg %d type %d", reg+1, LowOpndRegType_ss);
#endif
            compileTable[index].physicalReg = PhysicalReg_Null;
        }
    }
}
//! bookkeeping when a VR is updated

//! invalidate contents of some physical registers, clear nullCheckDone, and update inMemory;
//! check whether there exist tranfer points for this bytecode, if yes, perform the transfer
int updateVirtualReg(int reg, LowOpndRegType pType) {
    int k;
    OpndSize size = getRegSize(pType);
    //WAS only invalidate xmm VRs for the following cases:
    //if def reaches a use of vA,xmm and (the def is not xmm or is misaligned xmm)
    //  invalidate "vA,xmm"
    invalidateVR(reg, pType);
    clearVRNullCheck(reg, size);
    clearVRBoundCheck(reg, size);
    if(pType == LowOpndRegType_fs || pType == LowOpndRegType_fs_s)
        setVRToMemory(reg, size);
    else {
        clearVRToMemory(reg, size);
    }
    for(k = 0; k < currentBB->num_xfer_points; k++) {
        if(currentBB->xferPoints[k].offsetPC == offsetPC &&
           currentBB->xferPoints[k].regNum == reg &&
           currentBB->xferPoints[k].physicalType == pType &&
           currentBB->xferPoints[k].xtype != XFER_MEM_TO_XMM) {
            //perform the corresponding action for the def
            PhysicalReg regAll;
            if(currentBB->xferPoints[k].xtype == XFER_DEF_IS_XMM) {
                //def at fs: content of xmm is out-dated
                //def at xmm: content of misaligned xmm is out-dated
                //invalidateXmmVR(currentBB->xferPoints[k].tableIndex);
#ifdef DEBUG_XFER_POINTS
                if(currentBB->xferPoints[k].dumpToXmm) ALOGI("XFER set_virtual_reg to xmm: xmm VR %d", reg);
#endif
                if(pType == LowOpndRegType_xmm)  {
#ifdef DEBUG_XFER_POINTS
                    ALOGI("XFER set_virtual_reg to memory: xmm VR %d", reg);
#endif
                    PhysicalReg regAll = (PhysicalReg)checkVirtualReg(reg, LowOpndRegType_xmm, 0 /* do not update*/);
                    dumpToMem(reg, LowOpndRegType_xmm, regAll);
                }
                if(currentBB->xferPoints[k].vr_gpl >= 0) { //
                }
                if(currentBB->xferPoints[k].vr_gph >= 0) {
                }
            }
            if((pType == LowOpndRegType_gp || pType == LowOpndRegType_ss) &&
               (currentBB->xferPoints[k].xtype == XFER_DEF_TO_MEM ||
                currentBB->xferPoints[k].xtype == XFER_DEF_TO_GP_MEM)) {
                //the defined gp VR already in register
                //invalidateXmmVR(currentBB->xferPoints[k].tableIndex);
                regAll = (PhysicalReg)checkVirtualReg(reg, pType, 0 /* do not update*/);
                dumpToMem(reg, pType, regAll);
#ifdef DEBUG_XFER_POINTS
                ALOGI("XFER set_virtual_reg to memory: gp VR %d", reg);
#endif
            }
            if((pType == LowOpndRegType_fs_s || pType == LowOpndRegType_ss) &&
               currentBB->xferPoints[k].xtype == XFER_DEF_TO_GP_MEM) {
            }
        }
    }
    return -1;
}
////////////////////////////////////////////////////////////////
//REGISTER ALLOCATION
int spillForHardReg(int regNum, int type);
void decreaseRefCount(int index);
int getFreeReg(int type, int reg, int indexToCompileTable);
PhysicalReg spillForLogicalReg(int type, int reg, int indexToCompileTable);
int unspillLogicalReg(int spill_index, int physicalReg);
int searchVirtualInfoOfBB(LowOpndRegType type, int regNum, BasicBlock_O1* bb);
bool isTemp8Bit(int type, int reg);
bool matchType(int typeA, int typeB);
int getNextAccess(int compileIndex);
void dumpCompileTable();

//! allocate a register for a variable

//!if no physical register is free, call spillForLogicalReg to free up a physical register;
//!if the variable is a temporary and it was spilled, call unspillLogicalReg to load from spill location to the allocated physical register;
//!if updateRefCount is true, reduce reference count of the variable by 1
int registerAlloc(int type, int reg, bool isPhysical, bool updateRefCount) {
#ifdef DEBUG_REGALLOC
    ALOGI("%p: try to allocate register %d type %d isPhysical %d", currentBB, reg, type, isPhysical);
#endif
    if(currentBB == NULL) {
        if(type & LowOpndRegType_virtual) return PhysicalReg_Null;
        if(isPhysical) return reg; //for helper functions
        return PhysicalReg_Null;
    }
    //ignore EDI, ESP, EBP (glue)
    if(isPhysical && (reg == PhysicalReg_EDI || reg == PhysicalReg_ESP ||
                      reg == PhysicalReg_EBP || reg == PhysicalReg_Null))
        return reg;

    int newType = convertType(type, reg, isPhysical);
    if(newType & LowOpndRegType_scratch) reg = reg - PhysicalReg_SCRATCH_1 + 1;
    int tIndex = searchCompileTable(newType, reg);
    if(tIndex < 0) {
      ALOGE("reg %d type %d not found in registerAlloc", reg, newType);
      return PhysicalReg_Null;
    }

    //physical register
    if(isPhysical) {
        if(allRegs[reg].isUsed) { //if used by a non hard-coded register
            spillForHardReg(reg, newType);
        }
        allRegs[reg].isUsed = true;
#ifdef DEBUG_REG_USED
        ALOGI("REGALLOC: allocate a reg %d", reg);
#endif
        compileTable[tIndex].physicalReg = reg;
        if(updateRefCount)
            decreaseRefCount(tIndex);
#ifdef DEBUG_REGALLOC
        ALOGI("REGALLOC: allocate register %d for logical register %d %d",
               compileTable[tIndex].physicalReg, reg, newType);
#endif
        return reg;
    }
    //already allocated
    if(compileTable[tIndex].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_REGALLOC
        ALOGI("already allocated to physical register %d", compileTable[tIndex].physicalReg);
#endif
        if(updateRefCount)
            decreaseRefCount(tIndex);
        return compileTable[tIndex].physicalReg;
    }

    //at this point, the logical register is not hard-coded and is mapped to Reg_Null
    //first check whether there is a free reg
    //if not, call spillForLogicalReg
    int index = getFreeReg(newType, reg, tIndex);
    if(index >= 0 && index < PhysicalReg_Null) {
        //update compileTable & allRegs
        compileTable[tIndex].physicalReg = allRegs[index].physicalReg;
        allRegs[index].isUsed = true;
#ifdef DEBUG_REG_USED
        ALOGI("REGALLOC: register %d is free", allRegs[index].physicalReg);
#endif
    } else {
        PhysicalReg allocR = spillForLogicalReg(newType, reg, tIndex);
        compileTable[tIndex].physicalReg = allocR;
    }
    if(compileTable[tIndex].spill_loc_index >= 0) {
        unspillLogicalReg(tIndex, compileTable[tIndex].physicalReg);
    }
    if(updateRefCount)
        decreaseRefCount(tIndex);
#ifdef DEBUG_REGALLOC
    ALOGI("REGALLOC: allocate register %d for logical register %d %d",
           compileTable[tIndex].physicalReg, reg, newType);
#endif
    return compileTable[tIndex].physicalReg;
}
//!a variable will use a physical register allocated for another variable

//!This is used when MOVE_OPT is on, it tries to alias a virtual register with a temporary to remove a move
int registerAllocMove(int reg, int type, bool isPhysical, int srcReg) {
    if(srcReg == PhysicalReg_EDI || srcReg == PhysicalReg_ESP || srcReg == PhysicalReg_EBP)
        ALOGE("can't move from srcReg EDI or ESP or EBP");
#ifdef DEBUG_REGALLOC
    ALOGI("in registerAllocMove: reg %d type %d srcReg %d", reg, type, srcReg);
#endif
    int newType = convertType(type, reg, isPhysical);
    if(newType & LowOpndRegType_scratch) reg = reg - PhysicalReg_SCRATCH_1 + 1;
    int index = searchCompileTable(newType, reg);
    if(index < 0) {
        ALOGE("reg %d type %d not found in registerAllocMove", reg, newType);
        return -1;
    }

    decreaseRefCount(index);
    compileTable[index].physicalReg = srcReg;
#ifdef DEBUG_REGALLOC
    ALOGI("REGALLOC: registerAllocMove %d for logical register %d %d",
           compileTable[index].physicalReg, reg, newType);
#endif
    return srcReg;
}

//! check whether a physical register is available to be used by a variable

//! data structures accessed:
//! 1> currentBB->infoBasicBlock[index].allocConstraintsSorted
//!    sorted from high count to low count
//! 2> currentBB->allocConstraintsSorted
//!    sorted from low count to high count
//! 3> allRegs: whether a physical register is available, indexed by PhysicalReg
//! NOTE: if a temporary variable is 8-bit, only %eax, %ebx, %ecx, %edx can be used
int getFreeReg(int type, int reg, int indexToCompileTable) {
    syncAllRegs();
    /* handles requests for xmm or ss registers */
    int k;
    if(((type & MASK_FOR_TYPE) == LowOpndRegType_xmm) ||
       ((type & MASK_FOR_TYPE) == LowOpndRegType_ss)) {
        for(k = PhysicalReg_XMM0; k <= PhysicalReg_XMM7; k++) {
            if(!allRegs[k].isUsed) return k;
        }
        return -1;
    }
#ifdef DEBUG_REGALLOC
    ALOGI("USED registers: ");
    for(k = 0; k < 8; k++)
        ALOGI("%d used: %d time freed: %d callee-saveld: %d", k, allRegs[k].isUsed,
             allRegs[k].freeTimeStamp, allRegs[k].isCalleeSaved);
    ALOGI("");
#endif

    /* a VR is requesting a physical register */
    if(isVirtualReg(type)) { //find a callee-saved register
        /* if VR is type GG, check the pre-allocated physical register first */
        bool isGGVR = compileTable[indexToCompileTable].gType == GLOBALTYPE_GG;
        if(isGGVR) {
            int regCandidateT = compileTable[indexToCompileTable].physicalReg_prev;
            if(!allRegs[regCandidateT].isUsed) return regCandidateT;
        }

        int index = searchVirtualInfoOfBB((LowOpndRegType)(type&MASK_FOR_TYPE), reg, currentBB);
        if(index < 0) {
            ALOGE("VR %d %d not found in infoBasicBlock of currentBB %d (num of VRs %d)",
                  reg, type, currentBB->bb_index, currentBB->num_regs);
            dvmAbort();
        }

        /* check allocConstraints for this VR,
           return an available physical register with the highest constraint > 0 */
        for(k = 0; k < 8; k++) {
            if(currentBB->infoBasicBlock[index].allocConstraintsSorted[k].count == 0) break;
            int regCandidateT = currentBB->infoBasicBlock[index].allocConstraintsSorted[k].physicalReg;
            assert(regCandidateT < PhysicalReg_Null);
            if(!allRegs[regCandidateT].isUsed) return regCandidateT;
        }

        /* WAS: return an available physical register with the lowest constraint
           NOW: consider a new factor (freeTime) when there is a tie
                if 2 available physical registers have the same number of constraints
                choose the one with smaller free time stamp */
        int currentCount = -1;
        int index1 = -1;
        int smallestTime = -1;
        for(k = 0; k < 8; k++) {
            int regCandidateT = currentBB->allocConstraintsSorted[k].physicalReg;
            assert(regCandidateT < PhysicalReg_Null);
            if(index1 >= 0 && currentBB->allocConstraintsSorted[k].count > currentCount)
                break; //candidate has higher count than index1
            if(!allRegs[regCandidateT].isUsed) {
                if(index1 < 0) {
                    index1 = k;
                    currentCount = currentBB->allocConstraintsSorted[k].count;
                    smallestTime = allRegs[regCandidateT].freeTimeStamp;
                } else if(allRegs[regCandidateT].freeTimeStamp < smallestTime) {
                    index1 = k;
                    smallestTime = allRegs[regCandidateT].freeTimeStamp;
                }
            }
        }
        if(index1 >= 0) return currentBB->allocConstraintsSorted[index1].physicalReg;
        return -1;
    }
    /* handle request from a temporary variable or a glue variable */
    else {
        bool is8Bit = isTemp8Bit(type, reg);

        /* if the temporary variable is linked to a VR and
              the VR is not yet allocated to any physical register */
        int vr_num = compileTable[indexToCompileTable].linkageToVR;
        if(vr_num >= 0) {
            int index3 = searchCompileTable(LowOpndRegType_gp | LowOpndRegType_virtual, vr_num);
            if(index3 < 0) {
                ALOGE("2 in tracing linkage to VR %d", vr_num);
                dvmAbort();
            }

            if(compileTable[index3].physicalReg == PhysicalReg_Null) {
                int index2 = searchVirtualInfoOfBB(LowOpndRegType_gp, vr_num, currentBB);
                if(index2 < 0) {
                    ALOGE("1 in tracing linkage to VR %d", vr_num);
                    dvmAbort();
                }
#ifdef DEBUG_REGALLOC
                ALOGI("in getFreeReg for temporary reg %d, trace the linkage to VR %d",
                     reg, vr_num);
#endif

                /* check allocConstraints on the VR
                   return an available physical register with the highest constraint > 0
                */
                for(k = 0; k < 8; k++) {
                    if(currentBB->infoBasicBlock[index2].allocConstraintsSorted[k].count == 0) break;
                    int regCandidateT = currentBB->infoBasicBlock[index2].allocConstraintsSorted[k].physicalReg;
#ifdef DEBUG_REGALLOC
                    ALOGI("check register %d with count %d", regCandidateT,
                          currentBB->infoBasicBlock[index2].allocConstraintsSorted[k].count);
#endif
                    /* if the requesting variable is 8 bit */
                    if(is8Bit && regCandidateT > PhysicalReg_EDX) continue;
                    assert(regCandidateT < PhysicalReg_Null);
                    if(!allRegs[regCandidateT].isUsed) return regCandidateT;
                }
            }
        }
        /* check allocConstraints of the basic block
           if 2 available physical registers have the same constraint count,
              return the non callee-saved physical reg */
        /* enhancement: record the time when a register is freed (freeTimeStamp)
                        the purpose is to reduce false dependency
           priority: constraint count, non callee-saved, time freed
               let x be the lowest constraint count
               set A be available callee-saved physical registers with count == x
               set B be available non callee-saved physical registers with count == x
               if set B is not null, return the one with smallest free time
               otherwise, return the one in A with smallest free time
           To ignore whether it is callee-saved, add all candidates to set A
        */
        int setAIndex[8];
        int num_A = 0;
        int setBIndex[8];
        int num_B = 0;
        int index1 = -1; //points to an available physical reg with lowest count
        int currentCount = -1;
        for(k = 0; k < 8; k++) {
            int regCandidateT = currentBB->allocConstraintsSorted[k].physicalReg;
            if(is8Bit && regCandidateT > PhysicalReg_EDX) continue;

            if(index1 >= 0 && currentBB->allocConstraintsSorted[k].count > currentCount)
                break; //candidate has higher count than index1
            assert(regCandidateT < PhysicalReg_Null);
            if(!allRegs[regCandidateT].isUsed) {
                /*To ignore whether it is callee-saved, add all candidates to set A */
                if(false) {//!allRegs[regCandidateT].isCalleeSaved) { //add to set B
                    setBIndex[num_B++] = k;
                } else { //add to set A
                    setAIndex[num_A++] = k;
                }
                if(index1 < 0) {
                    /* index1 points to a physical reg with lowest count */
                    index1 = k;
                    currentCount = currentBB->allocConstraintsSorted[k].count;
                }
            }
        }

        int kk;
        int smallestTime = -1;
        index1 = -1;
        for(kk = 0; kk < num_B; kk++) {
            k = setBIndex[kk];
            int regCandidateT = currentBB->allocConstraintsSorted[k].physicalReg;
            assert(regCandidateT < PhysicalReg_Null);
            if(kk == 0 || allRegs[regCandidateT].freeTimeStamp < smallestTime) {
                index1 = k;
                smallestTime = allRegs[regCandidateT].freeTimeStamp;
            }
        }
        if(index1 >= 0)
            return currentBB->allocConstraintsSorted[index1].physicalReg;
        index1 = -1;
        for(kk = 0; kk < num_A; kk++) {
            k = setAIndex[kk];
            int regCandidateT = currentBB->allocConstraintsSorted[k].physicalReg;
            if(kk == 0 || allRegs[regCandidateT].freeTimeStamp < smallestTime) {
                index1 = k;
                smallestTime = allRegs[regCandidateT].freeTimeStamp;
            }
        }
        if(index1 >= 0) return currentBB->allocConstraintsSorted[index1].physicalReg;
        return -1;
    }
    return -1;
}

//! find a candidate physical register for a variable and spill all variables that are mapped to the candidate

//!
PhysicalReg spillForLogicalReg(int type, int reg, int indexToCompileTable) {
    //choose a used register to spill
    //when a GG virtual register is spilled, write it to interpretd stack, set physicalReg to Null
    //  at end of the basic block, load spilled GG VR to physical reg
    //when other types of VR is spilled, write it to interpreted stack, set physicalReg to Null
    //when a temporary (non-virtual) register is spilled, write it to stack, set physicalReg to Null
    //can we spill a hard-coded temporary register? YES
    int k, k2;
    PhysicalReg allocR;

    //do not try to free a physical reg that is used by more than one logical registers
    //fix on sep 28, 2009
    //do not try to spill a hard-coded logical register
    //do not try to free a physical reg that is outside of the range for 8-bit logical reg
    /* for each physical register,
       collect number of non-hardcode entries that are mapped to the physical register */
    int numOfUses[PhysicalReg_Null];
    for(k = PhysicalReg_EAX; k < PhysicalReg_Null; k++)
        numOfUses[k] = 0;
    for(k = 0; k < num_compile_entries; k++) {
        if((compileTable[k].physicalReg != PhysicalReg_Null) &&
           matchType(type, compileTable[k].physicalType) &&
           (compileTable[k].physicalType & LowOpndRegType_hard) == 0) {
            numOfUses[compileTable[k].physicalReg]++;
        }
    }

    /* candidates: all non-hardcode entries that are mapped to
           a physical register that is used by only one entry*/
    bool is8Bit = isTemp8Bit(type, reg);
    int candidates[COMPILE_TABLE_SIZE];
    int num_cand = 0;
    for(k = 0; k < num_compile_entries; k++) {
        if(matchType(type, compileTable[k].physicalType) &&
           compileTable[k].physicalReg != PhysicalReg_Null) {
            if(is8Bit && compileTable[k].physicalReg > PhysicalReg_EDX) continue; //not a candidate
            if(!canSpillReg[compileTable[k].physicalReg]) continue; //not a candidate
            if((compileTable[k].physicalType & LowOpndRegType_hard) == 0 &&
               numOfUses[compileTable[k].physicalReg] <= 1) {
                candidates[num_cand++] = k;
            }
        }
    }

    /* go through all candidates:
       first check GLUE-related entries */
    int spill_index = -1;
    for(k2 = 0; k2 < num_cand; k2++) {
        k = candidates[k2];
        if((compileTable[k].physicalReg != PhysicalReg_Null) &&
           matchType(type, compileTable[k].physicalType) &&
           (compileTable[k].regNum >= PhysicalReg_GLUE_DVMDEX &&
            compileTable[k].regNum != PhysicalReg_GLUE)) {
            allocR = (PhysicalReg)spillLogicalReg(k, true);
#ifdef DEBUG_REGALLOC
            ALOGI("SPILL register used by num %d type %d it is a GLUE register with refCount %d",
                  compileTable[k].regNum, compileTable[k].physicalType, compileTable[k].refCount);
#endif
            return allocR;
        }
    }

    /* out of the candates, find a VR that has the furthest next use */
    int furthestUse = offsetPC;
    for(k2 = 0; k2 < num_cand; k2++) {
        k = candidates[k2];
        if((compileTable[k].physicalReg != PhysicalReg_Null) &&
           matchType(type, compileTable[k].physicalType) &&
           isVirtualReg(compileTable[k].physicalType)) {
            int nextUse = getNextAccess(k);
            if(spill_index < 0 || nextUse > furthestUse) {
                spill_index = k;
                furthestUse = nextUse;
            }
        }
    }

    /* spill the VR with the furthest next use */
    if(spill_index >= 0) {
        allocR = (PhysicalReg)spillLogicalReg(spill_index, true);
        return allocR; //the register is still being used
    }

    /* spill an entry with the smallest refCount */
    int baseLeftOver = 0;
    int index = -1;
    for(k2 = 0; k2 < num_cand; k2++) {
        k = candidates[k2];
        if(k != indexForGlue &&
           (compileTable[k].physicalReg != PhysicalReg_Null) &&
           (compileTable[k].physicalType & LowOpndRegType_hard) == 0 && //not hard-coded
           matchType(type, compileTable[k].physicalType)) {
            if((index < 0) || (compileTable[k].refCount < baseLeftOver)) {
                baseLeftOver = compileTable[k].refCount;
                index = k;
            }
        }
    }
    if(index < 0) {
        dumpCompileTable();
        ALOGE("no register to spill for logical %d %d", reg, type);
        dvmAbort();
    }
    allocR = (PhysicalReg)spillLogicalReg(index, true);
#ifdef DEBUG_REGALLOC
    ALOGI("SPILL register used by num %d type %d it is a temporary register with refCount %d",
           compileTable[index].regNum, compileTable[index].physicalType, compileTable[index].refCount);
#endif
    return allocR;
}
//!spill a variable to memory, the variable is specified by an index to compileTable

//!If the variable is a temporary, get a spill location that is not in use and spill the content to the spill location;
//!If updateTable is true, set physicalReg to Null;
//!Return the physical register that was allocated to the variable
int spillLogicalReg(int spill_index, bool updateTable) {
    if((compileTable[spill_index].physicalType & LowOpndRegType_hard) != 0) {
        ALOGE("can't spill a hard-coded register");
        dvmAbort();
    }
    int physicalReg = compileTable[spill_index].physicalReg;
    if(!canSpillReg[physicalReg]) {
#ifdef PRINT_WARNING
        ALOGW("can't spill register %d", physicalReg);
#endif
        //dvmAbort(); //this happens in get_virtual_reg where VR is allocated to the same reg as the hardcoded temporary
    }
    if(isVirtualReg(compileTable[spill_index].physicalType)) {
        //spill back to memory
        dumpToMem(compileTable[spill_index].regNum,
                  (LowOpndRegType)(compileTable[spill_index].physicalType&MASK_FOR_TYPE),
                  compileTable[spill_index].physicalReg);
    }
    else {
        //update spill_loc_index
        int k = getSpillIndex(spill_index == indexForGlue,
                    getRegSize(compileTable[spill_index].physicalType));
        compileTable[spill_index].spill_loc_index = 4*k;
        if(k >= 0)
            spillIndexUsed[k] = 1;
        saveToSpillRegion(getRegSize(compileTable[spill_index].physicalType),
                          compileTable[spill_index].physicalReg, 4*k);
    }
    //compileTable[spill_index].physicalReg_prev = compileTable[spill_index].physicalReg;
#ifdef DEBUG_REGALLOC
    ALOGI("REGALLOC: SPILL logical reg %d %d with refCount %d allocated to %d",
           compileTable[spill_index].regNum,
           compileTable[spill_index].physicalType, compileTable[spill_index].refCount,
           compileTable[spill_index].physicalReg);
#endif
    if(!updateTable) return PhysicalReg_Null;

    int allocR = compileTable[spill_index].physicalReg;
    compileTable[spill_index].physicalReg = PhysicalReg_Null;
    return allocR;
}
//! load a varible from memory to physical register, the variable is specified with an index to compileTable

//!If the variable is a temporary, load from spill location and set the flag for the spill location to not used
int unspillLogicalReg(int spill_index, int physicalReg) {
    //can't un-spill to %eax in afterCall!!!
    //what if GG VR is allocated to %eax!!!
    if(isVirtualReg(compileTable[spill_index].physicalType)) {
        get_virtual_reg_noalloc(compileTable[spill_index].regNum,
                                getRegSize(compileTable[spill_index].physicalType),
                                physicalReg, true);
    }
    else {
        loadFromSpillRegion(getRegSize(compileTable[spill_index].physicalType),
                            physicalReg, compileTable[spill_index].spill_loc_index);
        spillIndexUsed[compileTable[spill_index].spill_loc_index >> 2] = 0;
        compileTable[spill_index].spill_loc_index = -1;
    }
#ifdef DEBUG_REGALLOC
    ALOGI("REGALLOC: UNSPILL logical reg %d %d with refCount %d", compileTable[spill_index].regNum,
           compileTable[spill_index].physicalType, compileTable[spill_index].refCount);
#endif
    return PhysicalReg_Null;
}

//!spill a virtual register to memory

//!if the current value of a VR is constant, write immediate to memory;
//!if the current value of a VR is in a physical register, call spillLogicalReg to dump content of the physical register to memory;
//!ifupdateTable is true, set the physical register for VR to Null and decrease reference count of the virtual register
int spillVirtualReg(int vrNum, LowOpndRegType type, bool updateTable) {
    int index = searchCompileTable(type | LowOpndRegType_virtual, vrNum);
    if(index < 0) {
        ALOGE("can't find VR %d %d in spillVirtualReg", vrNum, type);
        return -1;
    }
    //check whether it is const
    int value[2];
    int isConst = isVirtualRegConstant(vrNum, type, value, false); //do not update refCount
    if(isConst == 1 || isConst == 3) {
        dumpImmToMem(vrNum, OpndSize_32, value[0]);
    }
    if(getRegSize(type) == OpndSize_64 && (isConst == 2 || isConst == 3)) {
        dumpImmToMem(vrNum+1, OpndSize_32, value[1]);
    }
    if(isConst != 3 && compileTable[index].physicalReg != PhysicalReg_Null)
        spillLogicalReg(index, updateTable);
    if(updateTable) decreaseRefCount(index);
    return -1;
}

//! spill variables that are mapped to physical register (regNum)

//!
int spillForHardReg(int regNum, int type) {
    //find an entry that uses the physical register
    int spill_index = -1;
    int k;
    for(k = 0; k < num_compile_entries; k++) {
        if(k != indexForGlue &&
           compileTable[k].physicalReg == regNum &&
           matchType(type, compileTable[k].physicalType)) {
            spill_index = k;
            if(compileTable[k].regNum == regNum && compileTable[k].physicalType == type)
                continue;
            if(inGetVR_num >= 0 && compileTable[k].regNum == inGetVR_num && compileTable[k].physicalType == (type | LowOpndRegType_virtual))
                continue;
#ifdef DEBUG_REGALLOC
            ALOGI("SPILL logical reg %d %d to free hard-coded reg %d %d",
                   compileTable[spill_index].regNum, compileTable[spill_index].physicalType,
                   regNum, type);
            if(compileTable[spill_index].physicalType & LowOpndRegType_hard) dumpCompileTable();
#endif
            assert(spill_index < COMPILE_TABLE_SIZE);
            spillLogicalReg(spill_index, true);
        }
    }
    return regNum;
}
////////////////////////////////////////////////////////////////
//! update allocConstraints of the current basic block

//! allocConstraints specify how many times a hardcoded register is used in this basic block
void updateCurrentBBWithConstraints(PhysicalReg reg) {
    if(reg > PhysicalReg_EBP) {
        ALOGE("register %d out of range in updateCurrentBBWithConstraints", reg);
    }
    currentBB->allocConstraints[reg].count++;
}
//! sort allocConstraints and save the result in allocConstraintsSorted

//! allocConstraints specify how many times a virtual register is linked to a hardcode register
//! it is updated in getVirtualRegInfo and merged by mergeEntry2
int sortAllocConstraint(RegAllocConstraint* allocConstraints,
                        RegAllocConstraint* allocConstraintsSorted, bool fromHighToLow) {
    int ii, jj;
    int num_sorted = 0;
    for(jj = 0; jj < 8; jj++) {
        //figure out where to insert allocConstraints[jj]
        int count = allocConstraints[jj].count;
        int regT = allocConstraints[jj].physicalReg;
        assert(regT < PhysicalReg_Null);
        int insertIndex = -1;
        for(ii = 0; ii < num_sorted; ii++) {
            int regT2 = allocConstraintsSorted[ii].physicalReg;
            assert(regT2 < PhysicalReg_Null);
            if(allRegs[regT].isCalleeSaved &&
               count == allocConstraintsSorted[ii].count) {
                insertIndex = ii;
                break;
            }
            if((!allRegs[regT].isCalleeSaved) &&
               count == allocConstraintsSorted[ii].count &&
               (!allRegs[regT2].isCalleeSaved)) { //skip until found one that is not callee-saved
                insertIndex = ii;
                break;
            }
            if((fromHighToLow && count > allocConstraintsSorted[ii].count) ||
               ((!fromHighToLow) && count < allocConstraintsSorted[ii].count)) {
                insertIndex = ii;
                break;
            }
        }
        if(insertIndex < 0) {
            allocConstraintsSorted[num_sorted].physicalReg = (PhysicalReg)regT;
            allocConstraintsSorted[num_sorted].count = count;
            num_sorted++;
        } else {
            for(ii = num_sorted-1; ii >= insertIndex; ii--) {
                allocConstraintsSorted[ii+1] = allocConstraintsSorted[ii];
            }
            allocConstraintsSorted[insertIndex] = allocConstraints[jj];
            num_sorted++;
        }
    } //for jj
#ifdef DEBUG_ALLOC_CONSTRAINT
    for(jj = 0; jj < 8; jj++) {
        if(allocConstraintsSorted[jj].count > 0)
            ALOGI("%d: register %d has count %d", jj, allocConstraintsSorted[jj].physicalReg, allocConstraintsSorted[jj].count);
    }
#endif
    return 0;
}
//! find the entry for a given virtual register in compileTable

//!
int findVirtualRegInTable(u2 vA, LowOpndRegType type, bool printError) {
    int k = searchCompileTable(type | LowOpndRegType_virtual, vA);
    if(k < 0 && printError) {
        ALOGE("findVirtualRegInTable virtual register %d type %d", vA, type);
        dvmAbort();
    }
    return k;
}

//! check whether a virtual register is constant

//! the value of the constant is stored in valuePtr; if updateRefCount is true & the VR is constant, reference count for the VR will be reduced by 1
int isVirtualRegConstant(int regNum, LowOpndRegType type, int* valuePtr, bool updateRefCount) {

    OpndSize size = getRegSize(type);
    int k;
    int indexL = -1;
    int indexH = -1;
    for(k = 0; k < num_const_vr; k++) {
#ifdef DEBUG_CONST
        ALOGI("constVRTable VR %d isConst %d value %x", constVRTable[k].regNum, constVRTable[k].isConst, constVRTable[k].value);
#endif
        if(constVRTable[k].regNum == regNum) {
            indexL = k;
            continue;
        }
        if(constVRTable[k].regNum == regNum + 1 && size == OpndSize_64) {
            indexH = k;
            continue;
        }
    }
    bool isConstL = false;
    bool isConstH = false;
    if(indexL >= 0) {
        isConstL = constVRTable[indexL].isConst;
    }
    if(size == OpndSize_64 && indexH >= 0) {
        isConstH = constVRTable[indexH].isConst;
    }

    if((isConstL || isConstH)) {
        if(size == OpndSize_64 && isConstH)
            valuePtr[1] = constVRTable[indexH].value;
        if(isConstL)
            *valuePtr = constVRTable[indexL].value;
    }
    if((isConstL && size == OpndSize_32) || (isConstL && isConstH)) {
        if(updateRefCount) {
            int indexOrig = searchCompileTable(type | LowOpndRegType_virtual, regNum);
            if(indexOrig < 0) ALOGE("can't find VR in isVirtualRegConstant num %d type %d", regNum, type);
            decreaseRefCount(indexOrig);
        }
#ifdef DEBUG_CONST
        ALOGI("VR %d %d is const case", regNum, type);
#endif
        return 3;
    }
    if(size == OpndSize_32) return 0;
    if(isConstL) return 1;
    if(isConstH) return 2;
    return 0;
}

//!update RegAccessType of virtual register vB given RegAccessType of vA

//!RegAccessType can be D, L, H
//!D means full definition, L means only lower-half is defined, H means only higher half is defined
//!we say a VR has no exposed usage in a basic block if the accessType is D or DU
//!we say a VR has exposed usage in a basic block if the accessType is not D nor DU
//!we say a VR has exposed usage in other basic blocks (hasOtherExposedUsage) if
//!  there exists another basic block where VR has exposed usage in that basic block
//!A can be U, D, L, H, UD, UL, UH, DU, LU, HU (merged result)
//!B can be U, D, UD, DU (an entry for the current bytecode)
//!input isAPartiallyOverlapB can be any value between -1 to 6
//!if A is xmm: gp B lower half of A, (isAPartiallyOverlapB is 1)
//!             gp B higher half of A, (isAPartiallyOverlapB is 2)
//!             lower half of A covers the higher half of xmm B  (isAPartiallyOverlapB is 4)
//!             higher half of A covers the lower half of xmm B   (isAPartiallyOverlapB is 3)
//!if A is gp:  A covers the lower half of xmm B, (isAPartiallyOverlapB is 5)
//!             A covers the higher half of xmm B (isAPartiallyOverlapB is 6)
RegAccessType updateAccess1(RegAccessType A, OverlapCase isAPartiallyOverlapB) {
    if(A == REGACCESS_D || A == REGACCESS_DU || A == REGACCESS_UD) {
        if(isAPartiallyOverlapB == OVERLAP_ALIGN) return REGACCESS_D;
        if(isAPartiallyOverlapB == OVERLAP_B_IS_LOW_OF_A || isAPartiallyOverlapB == OVERLAP_B_IS_HIGH_OF_A)
            return REGACCESS_D;
        if(isAPartiallyOverlapB == OVERLAP_LOW_OF_A_IS_HIGH_OF_B || isAPartiallyOverlapB == OVERLAP_A_IS_LOW_OF_B)
            return REGACCESS_L;
        return REGACCESS_H;
    }
    if(A == REGACCESS_L || A == REGACCESS_LU || A == REGACCESS_UL) {
        if(isAPartiallyOverlapB == OVERLAP_ALIGN || isAPartiallyOverlapB == OVERLAP_A_IS_LOW_OF_B)
            return REGACCESS_L;
        if(isAPartiallyOverlapB == OVERLAP_B_IS_LOW_OF_A) return REGACCESS_D;
        if(isAPartiallyOverlapB == OVERLAP_B_IS_HIGH_OF_A || isAPartiallyOverlapB == OVERLAP_LOW_OF_A_IS_HIGH_OF_B)
            return REGACCESS_N;
        if(isAPartiallyOverlapB == OVERLAP_HIGH_OF_A_IS_LOW_OF_B || isAPartiallyOverlapB == OVERLAP_A_IS_HIGH_OF_B)
            return REGACCESS_H;
    }
    if(A == REGACCESS_H || A == REGACCESS_HU || A == REGACCESS_UH) {
        if(isAPartiallyOverlapB == OVERLAP_ALIGN || isAPartiallyOverlapB == OVERLAP_A_IS_HIGH_OF_B)
            return REGACCESS_H;
        if(isAPartiallyOverlapB == OVERLAP_B_IS_LOW_OF_A || isAPartiallyOverlapB == OVERLAP_HIGH_OF_A_IS_LOW_OF_B)
            return REGACCESS_N;
        if(isAPartiallyOverlapB == OVERLAP_B_IS_HIGH_OF_A) return REGACCESS_D;
        if(isAPartiallyOverlapB == OVERLAP_LOW_OF_A_IS_HIGH_OF_B || isAPartiallyOverlapB == OVERLAP_A_IS_LOW_OF_B)
            return REGACCESS_L;
    }
    return REGACCESS_N;
}
//! merge RegAccessType C1 with RegAccessType C2

//!C can be N,L,H,D
RegAccessType updateAccess2(RegAccessType C1, RegAccessType C2) {
    if(C1 == REGACCESS_D || C2 == REGACCESS_D) return REGACCESS_D;
    if(C1 == REGACCESS_N) return C2;
    if(C2 == REGACCESS_N) return C1;
    if(C1 == REGACCESS_L && C2 == REGACCESS_H) return REGACCESS_D;
    if(C1 == REGACCESS_H && C2 == REGACCESS_L) return REGACCESS_D;
    return C1;
}
//! merge RegAccessType C with RegAccessType B

//!C can be N,L,H,D
//!B can be U, D, UD, DU
RegAccessType updateAccess3(RegAccessType C, RegAccessType B) {
    if(B == REGACCESS_D || B == REGACCESS_DU) return B; //no exposed usage
    if(B == REGACCESS_U || B == REGACCESS_UD) {
        if(C == REGACCESS_N) return B;
        if(C == REGACCESS_L) return REGACCESS_LU;
        if(C == REGACCESS_H) return REGACCESS_HU;
        if(C == REGACCESS_D) return REGACCESS_DU;
    }
    return B;
}
//! merge RegAccessType A with RegAccessType B

//!argument isBPartiallyOverlapA can be any value between -1 and 2
//!0 means fully overlapping, 1 means B is the lower half, 2 means B is the higher half
RegAccessType mergeAccess2(RegAccessType A, RegAccessType B, OverlapCase isBPartiallyOverlapA) {
    if(A == REGACCESS_UD || A == REGACCESS_UL || A == REGACCESS_UH ||
       A == REGACCESS_DU || A == REGACCESS_LU || A == REGACCESS_HU) return A;
    if(A == REGACCESS_D) {
        if(B == REGACCESS_D) return REGACCESS_D;
        if(B == REGACCESS_U) return REGACCESS_DU;
        if(B == REGACCESS_UD) return REGACCESS_DU;
        if(B == REGACCESS_DU) return B;
    }
    if(A == REGACCESS_U) {
        if(B == REGACCESS_D && isBPartiallyOverlapA == OVERLAP_B_COVER_LOW_OF_A) return REGACCESS_UL;
        if(B == REGACCESS_D && isBPartiallyOverlapA == OVERLAP_B_COVER_HIGH_OF_A) return REGACCESS_UH;
        if(B == REGACCESS_D && (isBPartiallyOverlapA == OVERLAP_B_COVER_A)) return REGACCESS_UD;
        if(B == REGACCESS_U) return A;
        if(B == REGACCESS_UD && isBPartiallyOverlapA == OVERLAP_B_COVER_LOW_OF_A) return REGACCESS_UL;
        if(B == REGACCESS_UD && isBPartiallyOverlapA == OVERLAP_B_COVER_HIGH_OF_A) return REGACCESS_UH;
        if(B == REGACCESS_UD && (isBPartiallyOverlapA == OVERLAP_B_COVER_A)) return REGACCESS_UD;
        if(B == REGACCESS_DU && isBPartiallyOverlapA == OVERLAP_B_COVER_LOW_OF_A) return REGACCESS_UL;
        if(B == REGACCESS_DU && isBPartiallyOverlapA == OVERLAP_B_COVER_HIGH_OF_A) return REGACCESS_UH;
        if(B == REGACCESS_DU && (isBPartiallyOverlapA == OVERLAP_B_COVER_A)) return REGACCESS_UD;
    }
    if(A == REGACCESS_L) {
        if(B == REGACCESS_D && isBPartiallyOverlapA == OVERLAP_B_COVER_LOW_OF_A) return REGACCESS_L;
        if(B == REGACCESS_D && isBPartiallyOverlapA == OVERLAP_B_COVER_HIGH_OF_A) return REGACCESS_D;
        if(B == REGACCESS_D && (isBPartiallyOverlapA == OVERLAP_B_COVER_A)) return REGACCESS_D;
        if(B == REGACCESS_U) return REGACCESS_LU;
        if(B == REGACCESS_UD) return REGACCESS_LU;
        if(B == REGACCESS_DU && isBPartiallyOverlapA == OVERLAP_B_COVER_LOW_OF_A) return REGACCESS_LU;
        if(B == REGACCESS_DU && isBPartiallyOverlapA == OVERLAP_B_COVER_HIGH_OF_A) return REGACCESS_DU;
        if(B == REGACCESS_DU && (isBPartiallyOverlapA == OVERLAP_B_COVER_A)) return REGACCESS_DU;
    }
    if(A == REGACCESS_H) {
        if(B == REGACCESS_D && isBPartiallyOverlapA == OVERLAP_B_COVER_LOW_OF_A) return REGACCESS_D;
        if(B == REGACCESS_D && isBPartiallyOverlapA == OVERLAP_B_COVER_HIGH_OF_A) return REGACCESS_H;
        if(B == REGACCESS_D && (isBPartiallyOverlapA == OVERLAP_B_COVER_A)) return REGACCESS_D;
        if(B == REGACCESS_U) return REGACCESS_HU;
        if(B == REGACCESS_UD) return REGACCESS_HU;
        if(B == REGACCESS_DU && isBPartiallyOverlapA == OVERLAP_B_COVER_LOW_OF_A) return REGACCESS_DU;
        if(B == REGACCESS_DU && isBPartiallyOverlapA == OVERLAP_B_COVER_HIGH_OF_A) return REGACCESS_HU;
        if(B == REGACCESS_DU && (isBPartiallyOverlapA == OVERLAP_B_COVER_A)) return REGACCESS_DU;
    }
    return REGACCESS_N;
}

//!determines which part of a use is from a given definition

//!reachingDefLive tells us which part of the def is live at this point
//!isDefPartiallyOverlapUse can be any value between -1 and 2
RegAccessType setAccessTypeOfUse(OverlapCase isDefPartiallyOverlapUse, RegAccessType reachingDefLive) {
    if(isDefPartiallyOverlapUse == OVERLAP_B_COVER_A)
        return reachingDefLive;
    if(isDefPartiallyOverlapUse == OVERLAP_B_COVER_LOW_OF_A) { //def covers the low half of use
        return REGACCESS_L;
    }
    if(isDefPartiallyOverlapUse == OVERLAP_B_COVER_HIGH_OF_A) {
        return REGACCESS_H;
    }
    return REGACCESS_N;
}

//! search currentBB->defUseTable to find a def for regNum at offsetPC

//!
DefUsePair* searchDefUseTable(int offsetPC, int regNum, LowOpndRegType pType) {
    DefUsePair* ptr = currentBB->defUseTable;
    while(ptr != NULL) {
        if(ptr->def.offsetPC == offsetPC &&
           ptr->def.regNum == regNum &&
           ptr->def.physicalType == pType) {
            return ptr;
        }
        ptr = ptr->next;
    }
    return NULL;
}
void printDefUseTable() {
    ALOGI("PRINT defUseTable --------");
    DefUsePair* ptr = currentBB->defUseTable;
    while(ptr != NULL) {
        ALOGI("  def @ %x of VR %d %d has %d uses", ptr->def.offsetPC,
              ptr->def.regNum, ptr->def.physicalType,
              ptr->num_uses);
        DefOrUseLink* ptr2 = ptr->uses;
        while(ptr2 != NULL) {
            ALOGI("    use @ %x of VR %d %d accessType %d", ptr2->offsetPC,
                  ptr2->regNum,
                  ptr2->physicalType,
                  ptr2->accessType);
            ptr2 = ptr2->next;
        }
        ptr = ptr->next;
    }
}
//! when a VR is used, check whether a transfer from memory to XMM is necessary

//!
int updateVRAtUse(int reg, LowOpndRegType pType, int regAll) {
    int k;
    for(k = 0; k < currentBB->num_xfer_points; k++) {
        if(currentBB->xferPoints[k].offsetPC == offsetPC &&
           currentBB->xferPoints[k].xtype == XFER_MEM_TO_XMM &&
           currentBB->xferPoints[k].regNum == reg &&
           currentBB->xferPoints[k].physicalType == pType) {
#ifdef DEBUG_XFER_POINTS
            ALOGI("XFER from memory to xmm %d", reg);
#endif
            move_mem_to_reg_noalloc(OpndSize_64,
                                    4*currentBB->xferPoints[k].regNum, PhysicalReg_FP, true,
                                    MemoryAccess_VR, currentBB->xferPoints[k].regNum,
                                    regAll, true);
        }
    }
    return 0;
}
///////////////////////////////////////////////////////////////////////////////
// DEAD/USELESS STATEMENT ELMINATION
// bytecodes can be removed if a bytecode has no side effect and the defs are not used
// this optimization is guarded with DSE_OPT
// currently, this optimization is not on, since it does not provide observable performance improvement
//     and it increases compilation time

/* we remove a maximal of 40 bytecodes within a single basic block */
#define MAX_NUM_DEAD_PC_IN_BB 40
int deadPCs[MAX_NUM_DEAD_PC_IN_BB];
int num_dead_pc = 0;
//! collect all PCs that can be removed

//! traverse each byte code in the current basic block and check whether it can be removed, if yes, update deadPCs
void getDeadStmts() {
    BasicBlock_O1* bb = currentBB;
    int k;
    num_dead_pc = 0;
    //traverse each bytecode in the basic block
    //update offsetPC, rPC & inst
    u2* rPC_start = (u2*)currentMethod->insns;
    MIR* mir;
    for(mir = bb->jitBasicBlock->firstMIRInsn; mir; mir = mir->next) {
        offsetPC = mir->seqNum;
        rPC = rPC_start + mir->offset;
        if(mir->dalvikInsn.opcode >= kNumPackedOpcodes) continue;
#ifdef DEBUG_DSE
        ALOGI("DSE: offsetPC %x", offsetPC);
#endif
        inst = FETCH(0);
        bool isDeadStmt = true;
        getVirtualRegInfo(infoByteCode);
        u2 inst_op = INST_INST(inst);
	//skip bytecodes with side effect
        if(inst_op != OP_CONST_STRING && inst_op != OP_CONST_STRING_JUMBO &&
           inst_op != OP_MOVE && inst_op != OP_MOVE_OBJECT &&
           inst_op != OP_MOVE_FROM16 && inst_op != OP_MOVE_OBJECT_FROM16 &&
           inst_op != OP_MOVE_16 && inst_op != OP_CONST_CLASS &&
           inst_op != OP_MOVE_OBJECT_16 && inst_op != OP_MOVE_WIDE &&
           inst_op != OP_MOVE_WIDE_FROM16 && inst_op != OP_MOVE_WIDE_16 &&
           inst_op != OP_MOVE_RESULT && inst_op != OP_MOVE_RESULT_OBJECT) {
            continue;
        }
        //some statements do not define any VR!!!
        int num_defs = 0;
        for(k = 0; k < num_regs_per_bytecode; k++) {
            if(infoByteCode[k].accessType == REGACCESS_D ||
               infoByteCode[k].accessType == REGACCESS_UD ||
               infoByteCode[k].accessType == REGACCESS_DU) { //search defUseTable
                num_defs++;
                DefUsePair* indexT = searchDefUseTable(offsetPC, infoByteCode[k].regNum, infoByteCode[k].physicalType);
                if(indexT == NULL) {
                    ALOGE("def at %x of VR %d %d not in table",
                           offsetPC, infoByteCode[k].regNum, infoByteCode[k].physicalType);
                    return;
                }
                if(indexT->num_uses > 0) {
                    isDeadStmt = false;
                    break;
                } else {
#ifdef DEBUG_DSE
                    ALOGI("DSE: num_uses is %d for def at %d for VR %d %d", indexT->num_uses,
                          offsetPC, infoByteCode[k].regNum, infoByteCode[k].physicalType);
#endif
                }
            }
        } //for k
        if(num_defs == 0) isDeadStmt = false;
        if(isDeadStmt && num_dead_pc < MAX_NUM_DEAD_PC_IN_BB) {
#ifdef DEBUG_DSE
            ALOGI("DSE: stmt at %x is dead", offsetPC);
#endif
            deadPCs[num_dead_pc++] = offsetPC;
        }
    } //for offsetPC
#ifdef DEBUG_DSE
    ALOGI("Dead Stmts: ");
    for(k = 0; k < num_dead_pc; k++) ALOGI("%x ", deadPCs[k]);
    ALOGI("");
#endif
}
//! entry point to remove dead statements

//! recursively call getDeadStmts and remove uses in defUseTable that are from a dead PC
//! until there is no change to number of dead PCs
void removeDeadDefs() {
    int k;
    int deadPCs_2[MAX_NUM_DEAD_PC_IN_BB];
    int num_dead_pc_2 = 0;
    getDeadStmts();
    if(num_dead_pc == 0) return;
    DefUsePair* ptr = NULL;
    DefOrUseLink* ptrUse = NULL;
    DefOrUseLink* ptrUse_prev = NULL;
    while(true) {
        //check all the uses in defUseTable and remove any use that is from a dead PC
        ptr = currentBB->defUseTable;
        while(ptr != NULL) {
            int k3;
            ptrUse = ptr->uses;
            ptrUse_prev = NULL;
            while(ptrUse != NULL) {
                bool isIn = false;
                for(k3 = 0; k3 < num_dead_pc; k3++) {
                    if(ptrUse->offsetPC == deadPCs[k3]) {
                        isIn = true;
                        break;
                    }
                }//k3
                if(!isIn) {
                    ptrUse_prev = ptrUse;
                    ptrUse = ptrUse->next; //next use
                }
                else {
                    //go to next use and remove ptrUse
#ifdef DEBUG_DSE
                    ALOGI("DSE: remove usage at offsetPC %d reached by def at %d", ptrUse->offsetPC,
                           ptr->def.offsetPC);
#endif
                    DefOrUseLink* nextP = ptrUse->next;
                    if(ptrUse == ptr->useTail) ptr->useTail = ptrUse_prev;
                    free(ptrUse);
                    if(ptrUse_prev == NULL) {
                        ptr->uses = nextP;
                    } else {
                        ptrUse_prev->next = nextP;
                    }
                    ptrUse = nextP; //do not update ptrUse_prev
                    ptr->num_uses--;
                }
            }//while ptrUse
            ptr = ptr->next;
        }//while ptr
	//save deadPCs in deadPCs_2
        num_dead_pc_2 = num_dead_pc;
        for(k = 0; k < num_dead_pc_2; k++)
            deadPCs_2[k] = deadPCs[k];
	//update deadPCs
        getDeadStmts();
	//if no change to number of dead PCs, break out of the while loop
        if(num_dead_pc_2 == num_dead_pc) break;
    }//while
#ifdef DEBUG_DSE
    ALOGI("DSE: DEAD STMTS: ");
    for(k = 0; k < num_dead_pc; k++) {
        ALOGI("%d ", deadPCs[k]);
    }
    ALOGI("");
#endif
}
/////////////////////////////////////////////////////////////
//!search memVRTable for a given virtual register

//!
int searchMemTable(int regNum) {
    int k;
    for(k = 0; k < num_memory_vr; k++) {
        if(memVRTable[k].regNum == regNum) {
            return k;
        }
    }
    ALOGW("in searchMemTable can't find VR %d num_memory_vr %d", regNum, num_memory_vr);
    return -1;
}
/////////////////////////////////////////////////////////////////////////
// A VR is already in memory && NULL CHECK
//!check whether the latest content of a VR is in memory

//!
bool isInMemory(int regNum, OpndSize size) {
    int indexL = searchMemTable(regNum);
    int indexH = -1;
    if(size == OpndSize_64) indexH = searchMemTable(regNum+1);
    if(indexL < 0) return false;
    if(size == OpndSize_64 && indexH < 0) return false;
    if(!memVRTable[indexL].inMemory) return false;
    if(size == OpndSize_64 && (!memVRTable[indexH].inMemory)) return false;
    return true;
}
//!set field inMemory of memVRTable to true

//!
void setVRToMemory(int regNum, OpndSize size) {
    int indexL = searchMemTable(regNum);
    int indexH = -1;
    if(size == OpndSize_64) indexH = searchMemTable(regNum+1);
    if(indexL < 0) {
        ALOGE("VR %d not in memVRTable", regNum);
        return;
    }
    memVRTable[indexL].inMemory = true;
    if(size == OpndSize_64) {
        if(indexH < 0) {
            ALOGE("VR %d not in memVRTable", regNum+1);
            return;
        }
        memVRTable[indexH].inMemory = true;
    }
}
//! check whether null check for a VR is performed previously

//!
bool isVRNullCheck(int regNum, OpndSize size) {
    if(size != OpndSize_32) {
        ALOGE("isVRNullCheck size should be 32");
        dvmAbort();
    }
    int indexL = searchMemTable(regNum);
    if(indexL < 0) {
        ALOGE("VR %d not in memVRTable", regNum);
        return false;
    }
    return memVRTable[indexL].nullCheckDone;
}
bool isVRBoundCheck(int vr_array, int vr_index) {
    int indexL = searchMemTable(vr_array);
    if(indexL < 0) {
        ALOGE("isVRBoundCheck: VR %d not in memVRTable", vr_array);
        return false;
    }
    if(memVRTable[indexL].boundCheck.indexVR == vr_index)
        return memVRTable[indexL].boundCheck.checkDone;
    return false;
}
//! set nullCheckDone in memVRTable to true

//!
void setVRNullCheck(int regNum, OpndSize size) {
    if(size != OpndSize_32) {
        ALOGE("setVRNullCheck size should be 32");
        dvmAbort();
    }
    int indexL = searchMemTable(regNum);
    if(indexL < 0) {
        ALOGE("VR %d not in memVRTable", regNum);
        return;
    }
    memVRTable[indexL].nullCheckDone = true;
}
void setVRBoundCheck(int vr_array, int vr_index) {
    int indexL = searchMemTable(vr_array);
    if(indexL < 0) {
        ALOGE("setVRBoundCheck: VR %d not in memVRTable", vr_array);
        return;
    }
    memVRTable[indexL].boundCheck.indexVR = vr_index;
    memVRTable[indexL].boundCheck.checkDone = true;
}
void clearVRBoundCheck(int regNum, OpndSize size) {
    int k;
    for(k = 0; k < num_memory_vr; k++) {
        if(memVRTable[k].regNum == regNum ||
           (size == OpndSize_64 && memVRTable[k].regNum == regNum+1)) {
            memVRTable[k].boundCheck.checkDone = false;
        }
        if(memVRTable[k].boundCheck.indexVR == regNum ||
           (size == OpndSize_64 && memVRTable[k].boundCheck.indexVR == regNum+1)) {
            memVRTable[k].boundCheck.checkDone = false;
        }
    }
}
//! set inMemory of memVRTable to false

//!
void clearVRToMemory(int regNum, OpndSize size) {
    int indexL = searchMemTable(regNum);
    int indexH = -1;
    if(size == OpndSize_64) indexH = searchMemTable(regNum+1);
    if(indexL >= 0) {
        memVRTable[indexL].inMemory = false;
    }
    if(size == OpndSize_64 && indexH >= 0) {
        memVRTable[indexH].inMemory = false;
    }
}
//! set nullCheckDone of memVRTable to false

//!
void clearVRNullCheck(int regNum, OpndSize size) {
    int indexL = searchMemTable(regNum);
    int indexH = -1;
    if(size == OpndSize_64) indexH = searchMemTable(regNum+1);
    if(indexL >= 0) {
        memVRTable[indexL].nullCheckDone = false;
    }
    if(size == OpndSize_64 && indexH >= 0) {
        memVRTable[indexH].nullCheckDone = false;
    }
}

//! Extend Virtual Register life

//! Requests that the life of a specific virtual register be extended. This ensures
//! that its mapping to a physical register won't be canceled while the extension
//! request is valid. NOTE: This does not support 64-bit values (when two adjacent
//! VRs are used)
//! @see cancelVRFreeDelayRequest
//! @see getVRFreeDelayRequested
//! @see VRFreeDelayFlags
//! @param regNum is the VR number
//! @param reason explains why freeing must be delayed. A single or combination
//! of VRFreeDelayFlags should be used.
//! @return negative value if request failed
int requestVRFreeDelay(int regNum, u4 reason) {
    //TODO Add 64-bit operand support when needed
    int indexL = searchMemTable(regNum);
    if(indexL >= 0) {
        memVRTable[indexL].delayFreeFlags |= reason;
    } else {
        ALOGE("requestVRFreeDelay: VR %d not in memVRTable", regNum);
    }
    return indexL;
}

//! Cancel request for virtual register life extension

//! Cancels any outstanding requests to extended liveness of VR. Additionally,
//! this ensures that if the VR is no longer life after this point, it will
//! no longer be associated with a physical register which can then be reused.
//! NOTE: This does not support 64-bit values (when two adjacent VRs are used)
//! @see requestVRFreeDelay
//! @see getVRFreeDelayRequested
//! @see VRFreeDelayFlags
//! @param regNum is the VR number
//! @param reason explains what freeing delay request should be canceled. A single
//! or combination of VRFreeDelayFlags should be used.
void cancelVRFreeDelayRequest(int regNum, u4 reason) {
    //TODO Add 64-bit operand support when needed
    bool needCallToFreeReg = false;
    int indexL = searchMemTable(regNum);
    if(indexL >= 0) {
        if((memVRTable[indexL].delayFreeFlags & reason) != VRDELAY_NONE) { // don't cancel delay if it wasn't requested
            memVRTable[indexL].delayFreeFlags ^= reason; // only cancel this particular reason, not all others
            if(memVRTable[indexL].delayFreeFlags == VRDELAY_NONE)
                needCallToFreeReg = true; // freeReg might want to free this VR now if there is no longer a valid delay
        }
    }
    if(needCallToFreeReg)
        freeReg(true);
}

//! Gets status of virtual register free delay request

//! Finds out if there was a delay request for freeing this VR.
//! NOTE: This does not support 64-bit values (when two adjacent VRs are used)
//! @see requestVRFreeDelay
//! @see cancelVRFreeDelayRequest
//! @param regNum is the VR number
//! @return true if VR has an active delay request
bool getVRFreeDelayRequested(int regNum) {
    //TODO Add 64-bit operand support when needed
    int indexL = searchMemTable(regNum);
    if(indexL >= 0) {
        if(memVRTable[indexL].delayFreeFlags != VRDELAY_NONE)
            return true;
        return false;
    }
    return false;
}

//! find the basic block that a bytecode is in

//!
BasicBlock_O1* findForOffset(int offset) {
    int k;
    for(k = 0; k < num_bbs_for_method; k++) {
        if(method_bbs_sorted[k]->pc_start <= offset && method_bbs_sorted[k]->pc_end > offset)
            return method_bbs_sorted[k];
    }
    return NULL;
}
void dump_CFG(Method* method);

int current_bc_size = -1;

//! check whether a virtual register is used in a basic block

//!
bool isUsedInBB(int regNum, int type, BasicBlock_O1* bb) {
    int k;
    for(k = 0; k < bb->num_regs; k++) {
        if(bb->infoBasicBlock[k].physicalType == (type&MASK_FOR_TYPE) && bb->infoBasicBlock[k].regNum == regNum)
            return true;
    }
    return false;
}
//! return the index to infoBasicBlock for a given virtual register

//! return -1 if not found
int searchVirtualInfoOfBB(LowOpndRegType type, int regNum, BasicBlock_O1* bb) {
    int k;
    for(k = 0; k < bb->num_regs; k++) {
        if(bb->infoBasicBlock[k].physicalType == type && bb->infoBasicBlock[k].regNum == regNum)
            return k;
    }
    return -1;
}
//! return the index to compileTable for a given virtual register

//! return -1 if not found
int searchCompileTable(int type, int regNum) { //returns the index
    int k;
    for(k = 0; k < num_compile_entries; k++) {
        if(compileTable[k].physicalType == type && compileTable[k].regNum == regNum)
            return k;
    }
    return -1;
}
//!check whether a physical register for a variable with typeA will work for another variable with typeB

//!Type LowOpndRegType_ss is compatible with type LowOpndRegType_xmm
bool matchType(int typeA, int typeB) {
    if((typeA & MASK_FOR_TYPE) == (typeB & MASK_FOR_TYPE)) return true;
    if((typeA & MASK_FOR_TYPE) == LowOpndRegType_ss &&
       (typeB & MASK_FOR_TYPE) == LowOpndRegType_xmm) return true;
    if((typeA & MASK_FOR_TYPE) == LowOpndRegType_xmm &&
       (typeB & MASK_FOR_TYPE) == LowOpndRegType_ss) return true;
    return false;
}
//!check whether a virtual register is used in the current bytecode

//!
bool isUsedInByteCode(int regNum, int type) {
    getVirtualRegInfo(infoByteCode);
    int k;
    for(k = 0; k < num_regs_per_bytecode; k++) {
        if(infoByteCode[k].physicalType == (type&MASK_FOR_TYPE) && infoByteCode[k].regNum == regNum)
            return true;
    }
    return false;
}
//! obsolete
bool defineFirst(int atype) {
    if(atype == REGACCESS_D || atype == REGACCESS_L || atype == REGACCESS_H || atype == REGACCESS_DU)
        return true;
    return false;
}
//!check whether a virtual register is updated in a basic block

//!
bool notUpdated(RegAccessType atype) {
    if(atype == REGACCESS_U) return true;
    return false;
}
//!check whether a virtual register has exposed usage within a given basic block

//!
bool hasExposedUsage2(BasicBlock_O1* bb, int index) {
    RegAccessType atype = bb->infoBasicBlock[index].accessType;
    if(atype == REGACCESS_D || atype == REGACCESS_L || atype == REGACCESS_H || atype == REGACCESS_DU)
        return false;
    return true;
}
//! return the spill location that is not used

//!
int getSpillIndex(bool isGLUE, OpndSize size) {
    if(isGLUE) return 0;
    int k;
    for(k = 1; k <= MAX_SPILL_JIT_IA-1; k++) {
        if(size == OpndSize_64) {
            if(k < MAX_SPILL_JIT_IA-1 && spillIndexUsed[k] == 0 && spillIndexUsed[k+1] == 0)
                return k;
        }
        else if(spillIndexUsed[k] == 0) {
            return k;
        }
    }
    ALOGE("can't find spill position in spillLogicalReg");
    return -1;
}
//!this is called before generating a native code, it sets entries in array canSpillReg to true

//!startNativeCode must be paired with endNativeCode
void startNativeCode(int vr_num, int vr_type) {
    int k;
    for(k = 0; k < PhysicalReg_Null; k++) {
        canSpillReg[k] = true;
    }
    inGetVR_num = vr_num;
    inGetVR_type = vr_type;
}
//! called right after generating a native code

//!It sets entries in array canSpillReg to true and reset inGetVR_num to -1
void endNativeCode() {
    int k;
    for(k = 0; k < PhysicalReg_Null; k++) {
        canSpillReg[k] = true;
    }
    inGetVR_num = -1;
}
//! set canSpillReg[physicalReg] to false

//!
void donotSpillReg(int physicalReg) {
    canSpillReg[physicalReg] = false;
}
//! set canSpillReg[physicalReg] to true

//!
void doSpillReg(int physicalReg) {
    canSpillReg[physicalReg] = true;
}
//! touch hardcoded register %ecx and reduce its reference count

//!
int touchEcx() {
    //registerAlloc will spill logical reg that is mapped to ecx
    //registerAlloc will reduce refCount
    registerAlloc(LowOpndRegType_gp, PhysicalReg_ECX, true, true);
    return 0;
}
//! touch hardcoded register %eax and reduce its reference count

//!
int touchEax() {
    registerAlloc(LowOpndRegType_gp, PhysicalReg_EAX, true, true);
    return 0;
}
int touchEsi() {
    registerAlloc(LowOpndRegType_gp, PhysicalReg_ESI, true, true);
    return 0;
}
int touchXmm1() {
    registerAlloc(LowOpndRegType_xmm, XMM_1, true, true);
    return 0;
}
int touchEbx() {
    registerAlloc(LowOpndRegType_gp, PhysicalReg_EBX, true, true);
    return 0;
}

//! touch hardcoded register %edx and reduce its reference count

//!
int touchEdx() {
    registerAlloc(LowOpndRegType_gp, PhysicalReg_EDX, true, true);
    return 0;
}

#ifdef HACK_FOR_DEBUG
//for debugging purpose, instructions are added at a certain place
bool hacked = false;
void hackBug() {
  if(!hacked && iget_obj_inst == 13) {
#if 0
    move_reg_to_reg_noalloc(OpndSize_32, PhysicalReg_EBX, true, PhysicalReg_ECX, true);
    //move from ebx to ecx & update compileTable for v3
    int tIndex = searchCompileTable(LowOpndRegType_virtual | LowOpndRegType_gp, 3);
    if(tIndex < 0) ALOGE("hack can't find VR3");
    compileTable[tIndex].physicalReg = PhysicalReg_ECX;
#else
    move_reg_to_mem_noalloc(OpndSize_32, PhysicalReg_EBX, true, 12, PhysicalReg_FP, true);
#endif
  }
}
void hackBug2() {
  if(!hacked && iget_obj_inst == 13) {
    dump_imm_mem_noalloc(Mnemonic_MOV, OpndSize_32, 0, 12, PhysicalReg_FP, true);
    hacked = true;
  }
}
#endif

//! this function is called before calling a helper function or a vm function
int beforeCall(const char* target) { //spill all live registers
    if(currentBB == NULL) return -1;

    /* special case for ncgGetEIP: this function only updates %edx */
    if(!strcmp(target, "ncgGetEIP")) {
        touchEdx();
        return -1;
    }

    /* these functions use %eax for the return value */
    if((!strcmp(target, "dvmInstanceofNonTrivial")) ||
       (!strcmp(target, "dvmUnlockObject")) ||
       (!strcmp(target, "dvmAllocObject")) ||
       (!strcmp(target, "dvmAllocArrayByClass")) ||
       (!strcmp(target, "dvmAllocPrimitiveArray")) ||
       (!strcmp(target, "dvmInterpHandleFillArrayData")) ||
       (!strcmp(target, "dvmFindInterfaceMethodInCache")) ||
       (!strcmp(target, "dvmNcgHandlePackedSwitch")) ||
       (!strcmp(target, "dvmNcgHandleSparseSwitch")) ||
       (!strcmp(target, "dvmCanPutArrayElement")) ||
       (!strcmp(target, "moddi3")) || (!strcmp(target, "divdi3")) ||
       (!strcmp(target, "execute_inline"))
       || (!strcmp(target, "dvmJitToPatchPredictedChain"))
       || (!strcmp(target, "dvmJitHandlePackedSwitch"))
       || (!strcmp(target, "dvmJitHandleSparseSwitch"))
       ) {
        touchEax();
    }

    //these two functions also use %edx for the return value
    if((!strcmp(target, "moddi3")) || (!strcmp(target, "divdi3"))) {
        touchEdx();
    }
    if((!strcmp(target, ".new_instance_helper"))) {
        touchEsi(); touchEax();
    }
#if defined(ENABLE_TRACING)
    if((!strcmp(target, "common_periodicChecks4"))) {
        touchEdx();
    }
#endif
    if((!strcmp(target, ".const_string_helper"))) {
        touchEcx(); touchEax();
    }
    if((!strcmp(target, ".check_cast_helper"))) {
        touchEbx(); touchEsi();
    }
    if((!strcmp(target, ".instance_of_helper"))) {
        touchEbx(); touchEsi(); touchEcx();
    }
    if((!strcmp(target, ".monitor_enter_helper"))) {
        touchEbx();
    }
    if((!strcmp(target, ".monitor_exit_helper"))) {
        touchEbx();
    }
    if((!strcmp(target, ".aget_wide_helper"))) {
        touchEbx(); touchEcx(); touchXmm1();
    }
    if((!strcmp(target, ".aget_helper")) || (!strcmp(target, ".aget_char_helper")) ||
       (!strcmp(target, ".aget_short_helper")) || (!strcmp(target, ".aget_bool_helper")) ||
       (!strcmp(target, ".aget_byte_helper"))) {
        touchEbx(); touchEcx(); touchEdx();
    }
    if((!strcmp(target, ".aput_helper")) || (!strcmp(target, ".aput_char_helper")) ||
       (!strcmp(target, ".aput_short_helper")) || (!strcmp(target, ".aput_bool_helper")) ||
       (!strcmp(target, ".aput_byte_helper")) || (!strcmp(target, ".aput_wide_helper"))) {
        touchEbx(); touchEcx(); touchEdx();
    }
    if((!strcmp(target, ".sput_helper")) || (!strcmp(target, ".sput_wide_helper"))) {
        touchEdx(); touchEax();
    }
    if((!strcmp(target, ".sget_helper"))) {
        touchEdx(); touchEcx();
    }
    if((!strcmp(target, ".sget_wide_helper"))) {
        touchEdx(); touchXmm1();
    }
    if((!strcmp(target, ".aput_obj_helper"))) {
        touchEdx(); touchEcx(); touchEax();
    }
    if((!strcmp(target, ".iput_helper")) || (!strcmp(target, ".iput_wide_helper"))) {
        touchEbx(); touchEcx(); touchEsi();
    }
    if((!strcmp(target, ".iget_helper"))) {
        touchEbx(); touchEcx(); touchEdx();
    }
    if((!strcmp(target, ".iget_wide_helper"))) {
        touchEbx(); touchEcx(); touchXmm1();
    }
    if((!strcmp(target, ".new_array_helper"))) {
        touchEbx(); touchEdx(); touchEax();
    }
    if((!strcmp(target, ".invoke_virtual_helper"))) {
        touchEbx(); touchEcx();
    }
    if((!strcmp(target, ".invoke_direct_helper"))) {
        touchEsi(); touchEcx();
    }
    if((!strcmp(target, ".invoke_super_helper"))) {
        touchEbx(); touchEcx();
    }
    if((!strcmp(target, ".invoke_interface_helper"))) {
        touchEbx(); touchEcx();
    }
    if((!strcmp(target, ".invokeMethodNoRange_5_helper")) ||
       (!strcmp(target, ".invokeMethodNoRange_4_helper"))) {
        touchEbx(); touchEsi(); touchEax(); touchEdx();
    }
    if((!strcmp(target, ".invokeMethodNoRange_3_helper"))) {
        touchEbx(); touchEsi(); touchEax();
    }
    if((!strcmp(target, ".invokeMethodNoRange_2_helper"))) {
        touchEbx(); touchEsi();
    }
    if((!strcmp(target, ".invokeMethodNoRange_1_helper"))) {
        touchEbx();
    }
    if((!strcmp(target, ".invokeMethodRange_helper"))) {
        touchEdx(); touchEsi();
    }
#ifdef DEBUG_REGALLOC
    ALOGI("enter beforeCall");
#endif
    if(!strncmp(target, ".invokeArgsDone", 15)) resetGlue(PhysicalReg_GLUE_DVMDEX);

    freeReg(true); //to avoid spilling dead logical registers
    int k;
    for(k = 0; k < num_compile_entries; k++) {
        /* before throwing an exception, if GLUE is spilled, load to %ebp
           this should happen at last */
        if(k == indexForGlue) continue;
        if(compileTable[k].physicalReg != PhysicalReg_Null &&
           (compileTable[k].physicalType & LowOpndRegType_hard) == 0) {
            /* handles non hardcoded variables that are in physical registers */
            if(!strcmp(target, "exception")) {
                /* before throwing an exception
                   update contents of all VRs in Java stack */
                if(!isVirtualReg(compileTable[k].physicalType)) continue;
                /* to have correct GC, we should update contents for L VRs as well */
                //if(compileTable[k].gType == GLOBALTYPE_L) continue;
            }
            if((!strcmp(target, ".const_string_resolve")) ||
               (!strcmp(target, ".static_field_resolve")) ||
               (!strcmp(target, ".inst_field_resolve")) ||
               (!strcmp(target, ".class_resolve")) ||
               (!strcmp(target, ".direct_method_resolve")) ||
               (!strcmp(target, ".virtual_method_resolve")) ||
               (!strcmp(target, ".static_method_resolve"))) {
               /* physical register %ebx will keep its content
                  but to have correct GC, we should dump content of a VR
                     that is mapped to %ebx */
                if(compileTable[k].physicalReg == PhysicalReg_EBX &&
                   (!isVirtualReg(compileTable[k].physicalType)))
                    continue;
            }
            if((!strncmp(target, "dvm", 3)) || (!strcmp(target, "moddi3")) ||
               (!strcmp(target, "divdi3")) ||
               (!strcmp(target, "fmod")) || (!strcmp(target, "fmodf"))) {
                /* callee-saved registers (%ebx, %esi, %ebp, %edi) will keep the content
                   but to have correct GC, we should dump content of a VR
                      that is mapped to a callee-saved register */
                if((compileTable[k].physicalReg == PhysicalReg_EBX ||
                    compileTable[k].physicalReg == PhysicalReg_ESI) &&
                   (!isVirtualReg(compileTable[k].physicalType)))
                    continue;
            }
#ifdef DEBUG_REGALLOC
            ALOGI("SPILL logical register %d %d in beforeCall",
                  compileTable[k].regNum, compileTable[k].physicalType);
#endif
            spillLogicalReg(k, true);
        }
    }
    if(indexForGlue >= 0 && !strcmp(target, "exception") &&
       compileTable[indexForGlue].physicalReg == PhysicalReg_Null) {
        unspillLogicalReg(indexForGlue, PhysicalReg_EBP); //load %ebp
    }
#ifdef DEBUG_REGALLOC
    ALOGI("exit beforeCall");
#endif
    return 0;
}
int getFreeReg(int type, int reg, int indexToCompileTable);
//! after calling a helper function or a VM function

//!
int afterCall(const char* target) { //un-spill
    if(currentBB == NULL) return -1;
    if(!strcmp(target, "ncgGetEIP")) return -1;

    return 0;
}
//! check whether a temporary is 8-bit

//!
bool isTemp8Bit(int type, int reg) {
    if(currentBB == NULL) return false;
    if(!isTemporary(type, reg)) return false;
    int k;
    for(k = 0; k < num_temp_regs_per_bytecode; k++) {
        if(infoByteCodeTemp[k].physicalType == type &&
           infoByteCodeTemp[k].regNum == reg) {
            return infoByteCodeTemp[k].is8Bit;
        }
    }
    ALOGE("isTemp8Bit %d %d", type, reg);
    return false;
}

/* functions to access live ranges of a VR
   Live range info is stored in memVRTable[].ranges, which is a linked list
*/
//! check whether a VR is live at the current bytecode

//!
bool isVRLive(int vA) {
    int index = searchMemTable(vA);
    if(index < 0) {
        ALOGE("couldn't find VR %d in memTable", vA);
        return false;
    }
    LiveRange* ptr = memVRTable[index].ranges;
    while(ptr != NULL) {
        if(offsetPC >= ptr->start && offsetPC <= ptr->end) return true;
        ptr = ptr->next;
    }
    return false;
}

//! check whether the current bytecode is the last access to a VR within a live range

//!for 64-bit VR, return true only when true for both low half and high half
bool isLastByteCodeOfLiveRange(int compileIndex) {
    int k = compileIndex;
    OpndSize tSize = getRegSize(compileTable[k].physicalType);
    int index;
    LiveRange* ptr = NULL;
    if(tSize == OpndSize_32) {
        /* check live ranges for the VR */
        index = searchMemTable(compileTable[k].regNum);
        if(index < 0) {
            ALOGE("couldn't find VR %d in memTable", compileTable[k].regNum);
            return false;
        }
        ptr = memVRTable[index].ranges;
        while(ptr != NULL) {
            if(offsetPC == ptr->end) return true;
            ptr = ptr->next;
        }
        return false;
    }
    /* size of the VR is 64 */
    /* check live ranges of the low half */
    index = searchMemTable(compileTable[k].regNum);
    bool tmpB = false;
    if(index < 0) {
        ALOGE("couldn't find VR %d in memTable", compileTable[k].regNum);
        return false;
    }
    ptr = memVRTable[index].ranges;
    while(ptr != NULL) {
        if(offsetPC == ptr->end) {
            tmpB = true;
            break;
        }
        ptr = ptr->next;
    }
    if(!tmpB) return false;
    /* check live ranges of the high half */
    index = searchMemTable(compileTable[k].regNum+1);
    if(index < 0) {
        ALOGE("couldn't find VR %d in memTable", compileTable[k].regNum+1);
        return false;
    }
    ptr = memVRTable[index].ranges;
    while(ptr != NULL) {
        if(offsetPC == ptr->end) {
            return true;
        }
        ptr = ptr->next;
    }
    return false;
}

//! check whether the current bytecode is in a live range that extends to end of a basic block

//!for 64 bit, return true if true for both low half and high half
bool reachEndOfBB(int compileIndex) {
    int k = compileIndex;
    OpndSize tSize = getRegSize(compileTable[k].physicalType);
    int index;
    bool retCode = false;
    /* check live ranges of the low half */
    index = searchMemTable(compileTable[k].regNum);
    if(index < 0) {
        ALOGE("couldn't find VR %d in memTable", compileTable[k].regNum);
        return false;
    }
    LiveRange* ptr = memVRTable[index].ranges;
    while(ptr != NULL) {
        if(offsetPC >= ptr->start &&
           offsetPC <= ptr->end) {
            if(ptr->end == currentBB->pc_end) {
                retCode = true;
            }
            break;
        }
        ptr = ptr->next;
    }
    if(!retCode) return false;
    if(tSize == OpndSize_32) return true;
    /* check live ranges of the high half */
    index = searchMemTable(compileTable[k].regNum+1);
    if(index < 0) {
        ALOGE("couldn't find VR %d in memTable", compileTable[k].regNum+1);
        return false;
    }
    ptr = memVRTable[index].ranges;
    while(ptr != NULL) {
        if(offsetPC >= ptr->start &&
           offsetPC <= ptr->end) {
            if(ptr->end == currentBB->pc_end) return true;
            return false;
        }
        ptr = ptr->next;
    }
#ifdef PRINT_WARNING
    ALOGW("offsetPC %d not in live range of VR %d", offsetPC, compileTable[k].regNum+1);
#endif
    return false;
}

//!check whether the current bytecode is the next to last access to a VR within a live range

//!for 64 bit, return true if true for both low half and high half
bool isNextToLastAccess(int compileIndex) {
    int k = compileIndex;
    OpndSize tSize = getRegSize(compileTable[k].physicalType);
    int index;
    /* check live ranges for the low half */
    bool retCode = false;
    index = searchMemTable(compileTable[k].regNum);
    if(index < 0) {
        ALOGE("couldn't find VR %d in memTable", compileTable[k].regNum);
        return false;
    }
    LiveRange* ptr = memVRTable[index].ranges;
    while(ptr != NULL) {
        int num_access = ptr->num_access;

        if(num_access < 2) {
           ptr = ptr->next;
           continue;
        }

        if(offsetPC == ptr->accessPC[num_access-2]) {
           retCode = true;
           break;
        }
        ptr = ptr->next;
    }
    if(!retCode) return false;
    if(tSize == OpndSize_32) return true;
    /* check live ranges for the high half */
    index = searchMemTable(compileTable[k].regNum+1);
    if(index < 0) {
        ALOGE("couldn't find VR %d in memTable", compileTable[k].regNum+1);
        return false;
    }
    ptr = memVRTable[index].ranges;
    while(ptr != NULL) {
        int num_access = ptr->num_access;

        if(num_access < 2) {
           ptr = ptr->next;
           continue;
        }

        if(offsetPC == ptr->accessPC[num_access-2]) return true;
        ptr = ptr->next;
    }
    return false;
}

/** return the start of the next live range
    if there does not exist a next live range, return pc_end of the basic block
    for 64 bits, return the larger one for low half and high half
    Assume live ranges are sorted in order
*/
int getNextLiveRange(int compileIndex) {
    int k = compileIndex;
    OpndSize tSize = getRegSize(compileTable[k].physicalType);
    /* check live ranges of the low half */
    int index;
    index = searchMemTable(compileTable[k].regNum);
    if(index < 0) {
        ALOGE("couldn't find VR %d in memTable", compileTable[k].regNum);
        return offsetPC;
    }
    bool found = false;
    int nextUse = offsetPC;
    LiveRange* ptr = memVRTable[index].ranges;
    while(ptr != NULL) {
        if(ptr->start > offsetPC) {
            nextUse = ptr->start;
            found = true;
            break;
        }
        ptr = ptr->next;
    }
    if(!found) return currentBB->pc_end;
    if(tSize == OpndSize_32) return nextUse;

    /* check live ranges of the high half */
    found = false;
    index = searchMemTable(compileTable[k].regNum+1);
    if(index < 0) {
        ALOGE("couldn't find VR %d in memTable", compileTable[k].regNum+1);
        return offsetPC;
    }
    int nextUse2 = offsetPC;
    ptr = memVRTable[index].ranges;
    while(ptr != NULL) {
        if(ptr->start > offsetPC) {
            nextUse2 = ptr->start;
            found = true;
            break;
        }
        ptr = ptr->next;
    }
    if(!found) return currentBB->pc_end;
    /* return the larger one */
    return (nextUse2 > nextUse ? nextUse2 : nextUse);
}

/** return the next access to a variable
    If variable is 64-bit, get the next access to the lower half and the high half
        return the eariler one
*/
int getNextAccess(int compileIndex) {
    int k = compileIndex;
    OpndSize tSize = getRegSize(compileTable[k].physicalType);
    int index, k3;
    /* check live ranges of the low half */
    index = searchMemTable(compileTable[k].regNum);
    if(index < 0) {
        ALOGE("couldn't find VR %d in memTable", compileTable[k].regNum);
        return offsetPC;
    }
    bool found = false;
    int nextUse = offsetPC;
    LiveRange* ptr = memVRTable[index].ranges;
    while(ptr != NULL) {
        if(offsetPC >= ptr->start &&
           offsetPC <= ptr->end) {
            /* offsetPC belongs to this live range */
            for(k3 = 0; k3 < ptr->num_access; k3++) {
                if(ptr->accessPC[k3] > offsetPC) {
                    nextUse = ptr->accessPC[k3];
                    break;
                }
            }
            found = true;
            break;
        }
        ptr = ptr->next;
    }
#ifdef PRINT_WARNING
    if(!found)
        ALOGW("offsetPC %d not in live range of VR %d", offsetPC, compileTable[k].regNum);
#endif
    if(tSize == OpndSize_32) return nextUse;

    /* check live ranges of the high half */
    found = false;
    index = searchMemTable(compileTable[k].regNum+1);
    if(index < 0) {
        ALOGE("couldn't find VR %d in memTable", compileTable[k].regNum+1);
        return offsetPC;
    }
    int nextUse2 = offsetPC;
    ptr = memVRTable[index].ranges;
    while(ptr != NULL) {
        if(offsetPC >= ptr->start &&
           offsetPC <= ptr->end) {
            for(k3 = 0; k3 < ptr->num_access; k3++) {
                if(ptr->accessPC[k3] > offsetPC) {
                    nextUse2 = ptr->accessPC[k3];
                    break;
                }
            }
            found = true;
            break;
        }
        ptr = ptr->next;
    }
#ifdef PRINT_WARNING
    if(!found) ALOGW("offsetPC %d not in live range of VR %d", offsetPC, compileTable[k].regNum+1);
#endif
    /* return the earlier one */
    if(nextUse2 < nextUse) return nextUse2;
    return nextUse;
}

/** free variables that are no longer in use
    free a temporary with reference count of zero
    will dump content of a GL VR to memory if necessary
*/
int freeReg(bool spillGL) {
    if(currentBB == NULL) return 0;
    int k;
    for(k = 0; k < num_compile_entries; k++) {
        if(compileTable[k].refCount == 0 && compileTable[k].physicalReg != PhysicalReg_Null) {
            /* check entries with reference count of zero and is mapped to a physical register */
            bool typeA = !isVirtualReg(compileTable[k].physicalType);
            bool freeCrit = true, delayFreeing = false;
            bool typeC = false, typeB = false, reachEnd = false;
            if(isVirtualReg(compileTable[k].physicalType)) {
                /* VRs in the compile table */

                /* Check if delay for freeing was requested for this VR */
                delayFreeing = getVRFreeDelayRequested(compileTable[k].regNum);

                freeCrit = isLastByteCodeOfLiveRange(k); /* last bytecode of a live range */
                reachEnd = reachEndOfBB(k); /* in a live range that extends to end of a basic block */
#ifdef DEBUG_LIVE_RANGE
                ALOGI("IN freeReg: VR %d offsetPC %x freecrit %d reachEnd %d nextToLast %d", compileTable[k].regNum, offsetPC, freeCrit, reachEnd, isNextToLastAccess(k));
#endif
                /* Bug: spilling of VRs after edi(rFP) is updated in RETURN bytecode
                        will cause variables for callee to be spilled to the caller stack frame and
                                                        to overwrite varaibles for caller
                */
                /* last bytecode of a live range reaching end of BB if not counting the fake usage at end */
                bool boolB = reachEnd && isNextToLastAccess(k);
                /* Bug: when a GG VR is checked at end of a basic block,
                        freeCrit will be true and physicalReg will be set to Null
                   Fix: change free condition from freeCrit to (freeCrit && offsetPC != currentBB->pc_end)
                */
                /* conditions to free a GG VR:
                       last bytecode of a live range reaching end of BB if not counting the fake usage at end && endsWithReturn
                       or
                       last bytecode of a live range && offsetPC != currentBB->pc_end
                           -> last bytecode of a live range not reaching end
                */
                typeC = ((freeCrit && offsetPC != currentBB->pc_end) ||
                         (currentBB->endsWithReturn && boolB)) &&
                        compileTable[k].gType == GLOBALTYPE_GG &&
                        !delayFreeing;
                /* conditions to free a L|GL VR:
                       last bytecode of a live range
                       or
                       last bytecode of a live range reaching end of BB if not counting the fake usage at end
                */
                typeB = (freeCrit || boolB) &&
                        (compileTable[k].gType != GLOBALTYPE_GG) &&
                        !delayFreeing;
            }
            if(typeA || typeB || typeC) {
#ifdef DEBUG_REGALLOC
                if(typeA)
                    ALOGI("FREE TEMP %d with type %d allocated to %d",
                           compileTable[k].regNum, compileTable[k].physicalType,
                           compileTable[k].physicalReg);
                else if(typeB)
                    ALOGI("FREE VR L|GL %d with type %d allocated to %d",
                           compileTable[k].regNum, compileTable[k].physicalType,
                           compileTable[k].physicalReg);
                else if(typeC)
                    ALOGI("FREE VR GG %d with type %d allocated to %d",
                           compileTable[k].regNum, compileTable[k].physicalType,
                           compileTable[k].physicalReg);
#endif
                bool dumpGL = false;
                if(compileTable[k].gType == GLOBALTYPE_GL && !reachEnd) {
                    /* if the live range does not reach end of basic block
                       and there exists a try block from offsetPC to the next live range
                           dump VR to interpreted stack */
                    int tmpPC = getNextLiveRange(k);
                    if(existATryBlock(currentMethod, offsetPC, tmpPC)) dumpGL = true;
                }
                /* if the live range reach end of basic block, dump VR to interpreted stack */
                if(compileTable[k].gType == GLOBALTYPE_GL && reachEnd) dumpGL = true;
                if(dumpGL) {
                    if(spillGL) {
#ifdef DEBUG_REGALLOC
                        ALOGI("SPILL VR GL %d %d", compileTable[k].regNum, compileTable[k].physicalType);
#endif
                        spillLogicalReg(k, true); //will dump VR to memory & update physicalReg
                    }
                }
                else
                     compileTable[k].physicalReg = PhysicalReg_Null;
            }
            if(typeA) {
                if(compileTable[k].spill_loc_index >= 0) {
                    /* update spill info for temporaries */
                    spillIndexUsed[compileTable[k].spill_loc_index >> 2] = 0;
                    compileTable[k].spill_loc_index = -1;
                    ALOGE("free a temporary register with TRSTATE_SPILLED");
                }
            }
        }
    }
    syncAllRegs(); //sync up allRegs (isUsed & freeTimeStamp) with compileTable
    return 0;
}

//! reduce the reference count by 1

//! input: index to compileTable
void decreaseRefCount(int index) {
#ifdef DEBUG_REFCOUNT
    ALOGI("REFCOUNT: %d in decreaseRefCount %d %d", compileTable[index].refCount,
            compileTable[index].regNum, compileTable[index].physicalType);
#endif
    compileTable[index].refCount--;
    if(compileTable[index].refCount < 0) {
        ALOGE("refCount is negative for REG %d %d", compileTable[index].regNum, compileTable[index].physicalType);
        dvmAbort();
    }
}
//! reduce the reference count of a VR by 1

//! input: reg & type
int updateRefCount(int reg, LowOpndRegType type) {
    if(currentBB == NULL) return 0;
    int index = searchCompileTable(LowOpndRegType_virtual | type, reg);
    if(index < 0) {
        ALOGE("virtual reg %d type %d not found in updateRefCount", reg, type);
        return -1;
    }
    decreaseRefCount(index);
    return 0;
}
//! reduce the reference count of a variable by 1

//! The variable is named with lowering module's naming mechanism
int updateRefCount2(int reg, int type, bool isPhysical) {
    if(currentBB == NULL) return 0;
    int newType = convertType(type, reg, isPhysical);
    if(newType & LowOpndRegType_scratch) reg = reg - PhysicalReg_SCRATCH_1 + 1;
    int index = searchCompileTable(newType, reg);
    if(index < 0) {
        ALOGE("reg %d type %d not found in updateRefCount", reg, newType);
        return -1;
    }
    decreaseRefCount(index);
    return 0;
}
//! check whether a glue variable is in physical register or spilled

//!
bool isGlueHandled(int glue_reg) {
    if(currentBB == NULL) return false;
    int index = searchCompileTable(LowOpndRegType_gp, glue_reg);
    if(index < 0) {
        ALOGE("glue reg %d not found in isGlueHandled", glue_reg);
        return -1;
    }
    if(compileTable[index].spill_loc_index >= 0 ||
       compileTable[index].physicalReg != PhysicalReg_Null) {
#ifdef DEBUG_GLUE
        ALOGI("GLUE isGlueHandled for %d returns true", glue_reg);
#endif
        return true;
    }
#ifdef DEBUG_GLUE
    ALOGI("GLUE isGlueHandled for %d returns false", glue_reg);
#endif
    return false;
}
//! reset the state of a glue variable to not existant (not in physical register nor spilled)

//!
void resetGlue(int glue_reg) {
    if(currentBB == NULL) return;
    int index = searchCompileTable(LowOpndRegType_gp, glue_reg);
    if(index < 0) {
        ALOGE("glue reg %d not found in resetGlue", glue_reg);
        return;
    }
#ifdef DEBUG_GLUE
    ALOGI("GLUE reset for %d", glue_reg);
#endif
    compileTable[index].physicalReg = PhysicalReg_Null;
    if(compileTable[index].spill_loc_index >= 0)
        spillIndexUsed[compileTable[index].spill_loc_index >> 2] = 0;
    compileTable[index].spill_loc_index = -1;
}
//! set a glue variable in a physical register allocated for a variable

//! Variable is using lowering module's naming convention
void updateGlue(int reg, bool isPhysical, int glue_reg) {
    if(currentBB == NULL) return;
    int index = searchCompileTable(LowOpndRegType_gp, glue_reg);
    if(index < 0) {
        ALOGE("glue reg %d not found in updateGlue", glue_reg);
        return;
    }
    /* find the compileTable entry for variable <reg, isPhysical> */
    int newType = convertType(LowOpndRegType_gp, reg, isPhysical);
    if(newType & LowOpndRegType_scratch) reg = reg - PhysicalReg_SCRATCH_1 + 1;
    int index2 = searchCompileTable(newType, reg);
    if(index2 < 0 || compileTable[index2].physicalReg == PhysicalReg_Null) {
        ALOGE("updateGlue reg %d type %d", reg, newType);
        return;
    }
#ifdef DEBUG_GLUE
    ALOGI("physical register for GLUE %d set to %d", glue_reg, compileTable[index2].physicalReg);
#endif
    compileTable[index].physicalReg = compileTable[index2].physicalReg;
    compileTable[index].spill_loc_index = -1;
}

//! check whether a virtual register is in a physical register

//! If updateRefCount is 0, do not update reference count;
//!If updateRefCount is 1, update reference count only when VR is in a physical register
//!If updateRefCount is 2, update reference count
int checkVirtualReg(int reg, LowOpndRegType type, int updateRefCount) {
    if(currentBB == NULL) return PhysicalReg_Null;
    int index = searchCompileTable(LowOpndRegType_virtual | type, reg);
    if(index < 0) {
        ALOGE("virtual reg %d type %d not found in checkVirtualReg", reg, type);
        return PhysicalReg_Null;
    }
    //reduce reference count
    if(compileTable[index].physicalReg != PhysicalReg_Null) {
        if(updateRefCount != 0) decreaseRefCount(index);
        return compileTable[index].physicalReg;
    }
    if(updateRefCount == 2) decreaseRefCount(index);
    return PhysicalReg_Null;
}
//!check whether a temporary can share the same physical register with a VR

//!This is called in get_virtual_reg
//!If this function returns false, new register will be allocated for this temporary
bool checkTempReg2(int reg, int type, bool isPhysical, int physicalRegForVR) {
    if(currentBB == NULL) return false;
    if(isPhysical) return false;

    int newType = convertType(type, reg, isPhysical);
    if(newType & LowOpndRegType_scratch) reg = reg - PhysicalReg_SCRATCH_1 + 1;
    int k;
    for(k = 0; k < num_temp_regs_per_bytecode; k++) {
        if(infoByteCodeTemp[k].physicalType == newType &&
           infoByteCodeTemp[k].regNum == reg) {
#ifdef DEBUG_MOVE_OPT
            ALOGI("MOVE_OPT checkTempRegs for %d %d returns %d %d",
                   reg, newType, infoByteCodeTemp[k].shareWithVR, infoByteCodeTemp[k].is8Bit);
#endif
            if(!infoByteCodeTemp[k].is8Bit) return infoByteCodeTemp[k].shareWithVR;
            //is8Bit true for gp type only
            if(!infoByteCodeTemp[k].shareWithVR) return false;
            //both true
            if(physicalRegForVR >= PhysicalReg_EAX && physicalRegForVR <= PhysicalReg_EDX) return true;
#ifdef DEBUG_MOVE_OPT
            ALOGI("MOVE_OPT registerAllocMove not used for 8-bit register");
#endif
            return false;
        }
    }
    ALOGE("checkTempReg2 %d %d", reg, newType);
    return false;
}
//!check whether a temporary can share the same physical register with a VR

//!This is called in set_virtual_reg
int checkTempReg(int reg, int type, bool isPhysical, int vrNum) {
    if(currentBB == NULL) return PhysicalReg_Null;

    int newType = convertType(type, reg, isPhysical);
    if(newType & LowOpndRegType_scratch) reg = reg - PhysicalReg_SCRATCH_1 + 1;
    int index = searchCompileTable(newType, reg);
    if(index < 0) {
        ALOGE("temp reg %d type %d not found in checkTempReg", reg, newType);
        return PhysicalReg_Null;
    }

    //a temporary register can share the same physical reg with a VR if registerAllocMove is called
    //this will cause problem with move bytecode
    //get_VR(v1, t1) t1 and v1 point to the same physical reg
    //set_VR(t1, v2) t1 and v2 point to the same physical reg
    //this will cause v1 and v2 point to the same physical reg
    //FIX: if this temp reg shares a physical reg with another reg
    if(compileTable[index].physicalReg != PhysicalReg_Null) {
        int k;
        for(k = 0; k < num_compile_entries; k++) {
            if(k == index) continue;
            if(compileTable[k].physicalReg == compileTable[index].physicalReg) {
                return PhysicalReg_Null; //will allocate a register for VR
            }
        }
        decreaseRefCount(index);
        return compileTable[index].physicalReg;
    }
    if(compileTable[index].spill_loc_index >= 0) {
        //registerAlloc will call unspillLogicalReg (load from memory)
#ifdef DEBUG_REGALLOC
        ALOGW("in checkTempReg, the temporary register %d %d was spilled", reg, type);
#endif
        int regAll = registerAlloc(type, reg, isPhysical, true/* updateRefCount */);
        return regAll;
    }
    return PhysicalReg_Null;
}
//!check whether a variable has exposed usage in a basic block

//!It calls hasExposedUsage2
bool hasExposedUsage(LowOpndRegType type, int regNum, BasicBlock_O1* bb) {
    int index = searchVirtualInfoOfBB(type, regNum, bb);
    if(index >= 0 && hasExposedUsage2(bb, index)) {
        return true;
    }
    return false;
}
//!check whether a variable has exposed usage in other basic blocks

//!
bool hasOtherExposedUsage(OpndSize size, int regNum, BasicBlock_O1* bb) {
    return true; //assume the worst case
}

//! handles constant VRs at end of a basic block

//!If a VR is constant at end of a basic block and (it has exposed usage in other basic blocks or reaches a GG VR), dump immediate to memory
void constVREndOfBB() {
    BasicBlock_O1* bb = currentBB;
    int k, k2;
    //go through GG VRs, update a bool array
    int constUsedByGG[MAX_CONST_REG];
    for(k = 0; k < num_const_vr; k++)
        constUsedByGG[k] = 0;
    for(k = 0; k < num_compile_entries; k++) {
        if(isVirtualReg(compileTable[k].physicalType) && compileTable[k].gType == GLOBALTYPE_GG) {
            OpndSize size = getRegSize(compileTable[k].physicalType);
            int regNum = compileTable[k].regNum;
            int indexL = -1;
            int indexH = -1;
            for(k2 = 0; k2 < num_const_vr; k2++) {
                if(constVRTable[k2].regNum == regNum) {
                    indexL = k2;
                    continue;
                }
                if(constVRTable[k2].regNum == regNum + 1 && size == OpndSize_64) {
                    indexH = k2;
                    continue;
                }
            }
            if(indexL >= 0) constUsedByGG[indexL] = 1;
            if(indexH >= 0) constUsedByGG[indexH] = 1;
        } //GG VR
    }
    for(k = 0; k < num_const_vr; k++) {
        if(!constVRTable[k].isConst) continue;
        bool hasExp = false;
        if(constUsedByGG[k] == 0)
            hasExp = hasOtherExposedUsage(OpndSize_32, constVRTable[k].regNum, bb);
        if(constUsedByGG[k] != 0 || hasExp) {
            dumpImmToMem(constVRTable[k].regNum, OpndSize_32, constVRTable[k].value);
            setVRToMemory(constVRTable[k].regNum, OpndSize_32);
#ifdef DEBUG_ENDOFBB
            ALOGI("ENDOFBB: exposed VR %d is const %d (%x)",
                  constVRTable[k].regNum, constVRTable[k].value, constVRTable[k].value);
#endif
        } else {
#ifdef DEBUG_ENDOFBB
            ALOGI("ENDOFBB: unexposed VR %d is const %d (%x)",
                  constVRTable[k].regNum, constVRTable[k].value, constVRTable[k].value);
#endif
        }
    }
}

//!handles GG VRs at end of a basic block

//!make sure all GG VRs are in pre-defined physical registers
void globalVREndOfBB(const Method* method) {
    //fix: freeReg first to write LL VR back to memory to avoid it gets overwritten by GG VRs
    freeReg(true);
    int k;
    //spill GG VR first if it is not mapped to the specific reg
    //release GLUE regs
    for(k = 0; k < num_compile_entries; k++) {
        if(compileTable[k].regNum >= PhysicalReg_GLUE_DVMDEX &&
           compileTable[k].regNum != PhysicalReg_GLUE) {
            compileTable[k].physicalReg = PhysicalReg_Null;
            compileTable[k].spill_loc_index = -1;
        }
        //if part of a GG VR is const, the physical reg is set to null
        if(isVirtualReg(compileTable[k].physicalType) &&
           compileTable[k].gType == GLOBALTYPE_GG && compileTable[k].physicalReg != PhysicalReg_Null &&
           compileTable[k].physicalReg != compileTable[k].physicalReg_prev) {
#ifdef DEBUG_ENDOFBB
            ALOGW("end of BB GG VR is not mapped to the specific reg: %d %d %d",
                  compileTable[k].regNum, compileTable[k].physicalType, compileTable[k].physicalReg);
            ALOGW("ENDOFBB SPILL VR %d %d", compileTable[k].regNum, compileTable[k].physicalType);
#endif
            spillLogicalReg(k, true); //the next section will load VR from memory to the specific reg
        }
    }
    syncAllRegs();
    for(k = 0; k < num_compile_entries; k++) {
        if(isVirtualReg(compileTable[k].physicalType)) {
            if(compileTable[k].gType == GLOBALTYPE_GG &&
               compileTable[k].physicalReg == PhysicalReg_Null && (!currentBB->endsWithReturn)) {
#ifdef DEBUG_ENDOFBB
                ALOGI("ENDOFBB GET GG VR %d %d to physical register %d", compileTable[k].regNum,
                      compileTable[k].physicalType, compileTable[k].physicalReg_prev);
#endif
                compileTable[k].physicalReg = compileTable[k].physicalReg_prev;
                if(allRegs[compileTable[k].physicalReg_prev].isUsed) {
                    ALOGE("physical register for GG VR is still used");
                }
                get_virtual_reg_noalloc(compileTable[k].regNum,
                                        getRegSize(compileTable[k].physicalType),
                                        compileTable[k].physicalReg_prev,
                                        true);
            }
        }//not const
    }
    if(indexForGlue >= 0 &&
        compileTable[indexForGlue].physicalReg == PhysicalReg_Null) {
        unspillLogicalReg(indexForGlue, PhysicalReg_EBP); //load %ebp
    }
}

//! get ready for the next version of a hard-coded register

//!set its physicalReg to Null and update its reference count
int nextVersionOfHardReg(PhysicalReg pReg, int refCount) {
    int indexT = searchCompileTable(LowOpndRegType_gp | LowOpndRegType_hard, pReg);
    if(indexT < 0)
        return -1;
    compileTable[indexT].physicalReg = PhysicalReg_Null;
#ifdef DEBUG_REFCOUNT
    ALOGI("REFCOUNT: to %d in nextVersionOfHardReg %d", refCount, pReg);
#endif
    compileTable[indexT].refCount = refCount;
    return 0;
}

/** update compileTable with bb->infoBasicBlock[k]
*/
void insertFromVirtualInfo(BasicBlock_O1* bb, int k) {
    int index = searchCompileTable(LowOpndRegType_virtual | bb->infoBasicBlock[k].physicalType, bb->infoBasicBlock[k].regNum);
    if(index < 0) {
        /* the virtual register is not in compileTable, insert it */
        index = num_compile_entries;
        compileTable[num_compile_entries].physicalType = (LowOpndRegType_virtual | bb->infoBasicBlock[k].physicalType);
        compileTable[num_compile_entries].regNum = bb->infoBasicBlock[k].regNum;
        compileTable[num_compile_entries].physicalReg = PhysicalReg_Null;
        compileTable[num_compile_entries].bb = bb;
        compileTable[num_compile_entries].indexToInfoBB = k;
        compileTable[num_compile_entries].spill_loc_index = -1;
        compileTable[num_compile_entries].gType = bb->infoBasicBlock[k].gType;
        num_compile_entries++;
        if(num_compile_entries >= COMPILE_TABLE_SIZE) {
            ALOGE("compileTable overflow");
            dvmAbort();
        }
    }
    /* re-set reference count of all VRs */
    compileTable[index].refCount = bb->infoBasicBlock[k].refCount;
    compileTable[index].accessType = bb->infoBasicBlock[k].accessType;
    if(compileTable[index].gType == GLOBALTYPE_GG)
        compileTable[index].physicalReg_prev = bb->infoBasicBlock[k].physicalReg_GG;
}

/** update compileTable with infoByteCodeTemp[k]
*/
void insertFromTempInfo(int k) {
    int index = searchCompileTable(infoByteCodeTemp[k].physicalType, infoByteCodeTemp[k].regNum);
    if(index < 0) {
        /* the temporary is not in compileTable, insert it */
        index = num_compile_entries;
        compileTable[num_compile_entries].physicalType = infoByteCodeTemp[k].physicalType;
        compileTable[num_compile_entries].regNum = infoByteCodeTemp[k].regNum;
        num_compile_entries++;
        if(num_compile_entries >= COMPILE_TABLE_SIZE) {
            ALOGE("compileTable overflow");
            dvmAbort();
        }
    }
    compileTable[index].physicalReg = PhysicalReg_Null;
    compileTable[index].refCount = infoByteCodeTemp[k].refCount;
    compileTable[index].linkageToVR = infoByteCodeTemp[k].linkageToVR;
    compileTable[index].gType = GLOBALTYPE_L;
    compileTable[index].spill_loc_index = -1;
}

/* insert a glue-related register GLUE_DVMDEX to compileTable */
void insertGlueReg() {
    compileTable[num_compile_entries].physicalType = LowOpndRegType_gp;
    compileTable[num_compile_entries].regNum = PhysicalReg_GLUE_DVMDEX;
    compileTable[num_compile_entries].refCount = 2;
    compileTable[num_compile_entries].physicalReg = PhysicalReg_Null;
    compileTable[num_compile_entries].bb = NULL;
    compileTable[num_compile_entries].spill_loc_index = -1;
    compileTable[num_compile_entries].accessType = REGACCESS_N;
    compileTable[num_compile_entries].linkageToVR = -1;
    compileTable[num_compile_entries].gType = GLOBALTYPE_L;

    num_compile_entries++;
    if(num_compile_entries >= COMPILE_TABLE_SIZE) {
        ALOGE("compileTable overflow");
        dvmAbort();
    }
}

/** print infoBasicBlock of the given basic block
*/
void dumpVirtualInfoOfBasicBlock(BasicBlock_O1* bb) {
    int jj;
    ALOGI("Virtual Info for BB%d --------", bb->bb_index);
    for(jj = 0; jj < bb->num_regs; jj++) {
        ALOGI("regNum %d physicalType %d accessType %d refCount %d def ",
               bb->infoBasicBlock[jj].regNum, bb->infoBasicBlock[jj].physicalType,
               bb->infoBasicBlock[jj].accessType, bb->infoBasicBlock[jj].refCount);
        int k;
        for(k = 0; k < bb->infoBasicBlock[jj].num_reaching_defs; k++)
            ALOGI("[%x %d %d %d] ", bb->infoBasicBlock[jj].reachingDefs[k].offsetPC,
                   bb->infoBasicBlock[jj].reachingDefs[k].regNum,
                   bb->infoBasicBlock[jj].reachingDefs[k].physicalType,
                   bb->infoBasicBlock[jj].reachingDefs[k].accessType);
        ALOGI("");
    }
}

/** print compileTable
*/
void dumpCompileTable() {
    int jj;
    ALOGI("Compile Table for method ----------");
    for(jj = 0; jj < num_compile_entries; jj++) {
        ALOGI("regNum %d physicalType %d refCount %d isConst %d physicalReg %d type %d",
               compileTable[jj].regNum, compileTable[jj].physicalType,
               compileTable[jj].refCount, compileTable[jj].isConst, compileTable[jj].physicalReg, compileTable[jj].gType);
    }
}

//!check whether a basic block is the start of an exception handler

//!
bool isFirstOfHandler(BasicBlock_O1* bb) {
    int i;
    for(i = 0; i < num_exception_handlers; i++) {
        if(bb->pc_start == exceptionHandlers[i]) return true;
    }
    return false;
}

//! create a basic block that starts at src_pc and ends at end_pc

//!
BasicBlock_O1* createBasicBlock(int src_pc, int end_pc) {
    BasicBlock_O1* bb = (BasicBlock_O1*)malloc(sizeof(BasicBlock_O1));
    if(bb == NULL) {
        ALOGE("out of memory");
        return NULL;
    }
    bb->pc_start = src_pc;
    bb->bb_index = num_bbs_for_method;
    if(bb_entry == NULL) bb_entry = bb;

    /* insert the basic block to method_bbs_sorted in ascending order of pc_start */
    int k;
    int index = -1;
    for(k = 0; k < num_bbs_for_method; k++)
        if(method_bbs_sorted[k]->pc_start > src_pc) {
            index = k;
            break;
        }
    if(index == -1)
        method_bbs_sorted[num_bbs_for_method] = bb;
    else {
        /* push the elements from index by 1 */
        for(k = num_bbs_for_method-1; k >= index; k--)
            method_bbs_sorted[k+1] = method_bbs_sorted[k];
        method_bbs_sorted[index] = bb;
    }
    num_bbs_for_method++;
    if(num_bbs_for_method >= MAX_NUM_BBS_PER_METHOD) {
        ALOGE("too many basic blocks");
        dvmAbort();
    }
    return bb;
}

/* BEGIN code to handle state transfers */
//! save the current state of register allocator to a state table

//!
void rememberState(int stateNum) {
#ifdef DEBUG_STATE
    ALOGI("STATE: remember state %d", stateNum);
#endif
    int k;
    for(k = 0; k < num_compile_entries; k++) {
        if(stateNum == 1) {
            stateTable1_1[k].physicalReg = compileTable[k].physicalReg;
            stateTable1_1[k].spill_loc_index = compileTable[k].spill_loc_index;
        }
        else if(stateNum == 2) {
            stateTable1_2[k].physicalReg = compileTable[k].physicalReg;
            stateTable1_2[k].spill_loc_index = compileTable[k].spill_loc_index;
        }
        else if(stateNum == 3) {
            stateTable1_3[k].physicalReg = compileTable[k].physicalReg;
            stateTable1_3[k].spill_loc_index = compileTable[k].spill_loc_index;
        }
        else if(stateNum == 4) {
            stateTable1_4[k].physicalReg = compileTable[k].physicalReg;
            stateTable1_4[k].spill_loc_index = compileTable[k].spill_loc_index;
        }
        else ALOGE("state table overflow");
#ifdef DEBUG_STATE
        ALOGI("logical reg %d %d mapped to physical reg %d with spill index %d refCount %d",
               compileTable[k].regNum, compileTable[k].physicalType, compileTable[k].physicalReg,
               compileTable[k].spill_loc_index, compileTable[k].refCount);
#endif
    }
    for(k = 0; k < num_memory_vr; k++) {
        if(stateNum == 1) {
            stateTable2_1[k].regNum = memVRTable[k].regNum;
            stateTable2_1[k].inMemory = memVRTable[k].inMemory;
        }
        else if(stateNum == 2) {
            stateTable2_2[k].regNum = memVRTable[k].regNum;
            stateTable2_2[k].inMemory = memVRTable[k].inMemory;
        }
        else if(stateNum == 3) {
            stateTable2_3[k].regNum = memVRTable[k].regNum;
            stateTable2_3[k].inMemory = memVRTable[k].inMemory;
        }
        else if(stateNum == 4) {
            stateTable2_4[k].regNum = memVRTable[k].regNum;
            stateTable2_4[k].inMemory = memVRTable[k].inMemory;
        }
        else ALOGE("state table overflow");
#ifdef DEBUG_STATE
        ALOGI("virtual reg %d in memory %d", memVRTable[k].regNum, memVRTable[k].inMemory);
#endif
    }
}

//!update current state of register allocator with a state table

//!
void goToState(int stateNum) {
    int k;
#ifdef DEBUG_STATE
    ALOGI("STATE: go to state %d", stateNum);
#endif
    for(k = 0; k < num_compile_entries; k++) {
        if(stateNum == 1) {
            compileTable[k].physicalReg = stateTable1_1[k].physicalReg;
            compileTable[k].spill_loc_index = stateTable1_1[k].spill_loc_index;
        }
        else if(stateNum == 2) {
            compileTable[k].physicalReg = stateTable1_2[k].physicalReg;
            compileTable[k].spill_loc_index = stateTable1_2[k].spill_loc_index;
        }
        else if(stateNum == 3) {
            compileTable[k].physicalReg = stateTable1_3[k].physicalReg;
            compileTable[k].spill_loc_index = stateTable1_3[k].spill_loc_index;
        }
        else if(stateNum == 4) {
            compileTable[k].physicalReg = stateTable1_4[k].physicalReg;
            compileTable[k].spill_loc_index = stateTable1_4[k].spill_loc_index;
        }
        else ALOGE("state table overflow");
    }
    updateSpillIndexUsed();
    syncAllRegs(); //to sync up allRegs CAN'T call freeReg here
    //since it will change the state!!!
    for(k = 0; k < num_memory_vr; k++) {
        if(stateNum == 1) {
            memVRTable[k].regNum = stateTable2_1[k].regNum;
            memVRTable[k].inMemory = stateTable2_1[k].inMemory;
        }
        else if(stateNum == 2) {
            memVRTable[k].regNum = stateTable2_2[k].regNum;
            memVRTable[k].inMemory = stateTable2_2[k].inMemory;
        }
        else if(stateNum == 3) {
            memVRTable[k].regNum = stateTable2_3[k].regNum;
            memVRTable[k].inMemory = stateTable2_3[k].inMemory;
        }
        else if(stateNum == 4) {
            memVRTable[k].regNum = stateTable2_4[k].regNum;
            memVRTable[k].inMemory = stateTable2_4[k].inMemory;
        }
        else ALOGE("state table overflow");
    }
}
typedef struct TransferOrder {
    int targetReg;
    int targetSpill;
    int compileIndex;
} TransferOrder;
#define MAX_NUM_DEST 20
//! a source register is used as a source in transfer
//! it can have a maximum of MAX_NUM_DEST destinations
typedef struct SourceReg {
    int physicalReg;
    int num_dests; //check bound
    TransferOrder dsts[MAX_NUM_DEST];
} SourceReg;
int num_src_regs = 0; //check bound
//! physical registers that are used as a source in transfer
//! we allow a maximum of MAX_NUM_DEST sources in a transfer
SourceReg srcRegs[MAX_NUM_DEST];
//! tell us whether a source register is handled already
bool handledSrc[MAX_NUM_DEST];
//! in what order should the source registers be handled
int handledOrder[MAX_NUM_DEST];
//! insert a source register with a single destination

//!
void insertSrcReg(int srcPhysical, int targetReg, int targetSpill, int index) {
    int k = 0;
    for(k = 0; k < num_src_regs; k++) {
        if(srcRegs[k].physicalReg == srcPhysical) { //increase num_dests
            if(srcRegs[k].num_dests >= MAX_NUM_DEST) {
                ALOGE("exceed number dst regs for a source reg");
                dvmAbort();
            }
            srcRegs[k].dsts[srcRegs[k].num_dests].targetReg = targetReg;
            srcRegs[k].dsts[srcRegs[k].num_dests].targetSpill = targetSpill;
            srcRegs[k].dsts[srcRegs[k].num_dests].compileIndex = index;
            srcRegs[k].num_dests++;
            return;
        }
    }
    if(num_src_regs >= MAX_NUM_DEST) {
        ALOGE("exceed number of source regs");
        dvmAbort();
    }
    srcRegs[num_src_regs].physicalReg = srcPhysical;
    srcRegs[num_src_regs].num_dests = 1;
    srcRegs[num_src_regs].dsts[0].targetReg = targetReg;
    srcRegs[num_src_regs].dsts[0].targetSpill = targetSpill;
    srcRegs[num_src_regs].dsts[0].compileIndex = index;
    num_src_regs++;
}
//! check whether a register is a source and the source is not yet handled

//!
bool dstStillInUse(int dstReg) {
    if(dstReg == PhysicalReg_Null) return false;
    int k;
    int index = -1;
    for(k = 0; k < num_src_regs; k++) {
        if(dstReg == srcRegs[k].physicalReg) {
            index = k;
            break;
        }
    }
    if(index < 0) return false; //not in use
    if(handledSrc[index]) return false; //not in use
    return true;
}
//! reset the state of glue variables in a state table

//!
void resetStateOfGlue(int stateNum, int k) {
#ifdef DEBUG_STATE
    ALOGI("resetStateOfGlue state %d regNum %d", stateNum, compileTable[k].regNum);
#endif
    if(stateNum == 1) {
        stateTable1_1[k].physicalReg = PhysicalReg_Null;
        stateTable1_1[k].spill_loc_index = -1;
    }
    else if(stateNum == 2) {
        stateTable1_2[k].physicalReg = PhysicalReg_Null;
        stateTable1_2[k].spill_loc_index = -1;
    }
    else if(stateNum == 3) {
        stateTable1_3[k].physicalReg = PhysicalReg_Null;
        stateTable1_3[k].spill_loc_index = -1;
    }
    else if(stateNum == 4) {
        stateTable1_4[k].physicalReg = PhysicalReg_Null;
        stateTable1_4[k].spill_loc_index = -1;
    }
}
//! construct a legal order of the source registers in this transfer

//!
void constructSrcRegs(int stateNum) {
    int k;
    num_src_regs = 0;
#ifdef DEBUG_STATE
    ALOGI("IN constructSrcRegs");
#endif

    for(k = 0; k < num_compile_entries; k++) {
#ifdef DEBUG_STATE
        ALOGI("logical reg %d %d mapped to physical reg %d with spill index %d refCount %d",
               compileTable[k].regNum, compileTable[k].physicalType, compileTable[k].physicalReg,
               compileTable[k].spill_loc_index, compileTable[k].refCount);
#endif

        int pType = compileTable[k].physicalType;
        //ignore hardcoded logical registers
        if((pType & LowOpndRegType_hard) != 0) continue;
        //ignore type _fs
        if((pType & MASK_FOR_TYPE) == LowOpndRegType_fs) continue;
        if((pType & MASK_FOR_TYPE) == LowOpndRegType_fs_s) continue;

        //GL VR refCount is zero, can't ignore
        //L VR refCount is zero, ignore
        //GG VR refCount is zero, can't ignore
        //temporary refCount is zero, ignore

        //for GLUE variables, if they do not exist, reset the entries in state table
        if(compileTable[k].physicalReg == PhysicalReg_Null &&
           compileTable[k].regNum >= PhysicalReg_GLUE_DVMDEX &&
           compileTable[k].regNum != PhysicalReg_GLUE &&
           compileTable[k].spill_loc_index < 0) {
            resetStateOfGlue(stateNum, k);
        }

        /* get the target state */
        int targetReg = PhysicalReg_Null;
        int targetSpill = -1;
        if(stateNum == 1) {
            targetReg = stateTable1_1[k].physicalReg;
            targetSpill = stateTable1_1[k].spill_loc_index;
        }
        else if(stateNum == 2) {
            targetReg = stateTable1_2[k].physicalReg;
            targetSpill = stateTable1_2[k].spill_loc_index;
        }
        else if(stateNum == 3) {
            targetReg = stateTable1_3[k].physicalReg;
            targetSpill = stateTable1_3[k].spill_loc_index;
        }
        else if(stateNum == 4) {
            targetReg = stateTable1_4[k].physicalReg;
            targetSpill = stateTable1_4[k].spill_loc_index;
        }

        /* there exists an ordering problem
           for example:
             for a VR, move from memory to a physical reg esi
             for a temporary regsiter, from esi to ecx
             if we handle VR first, content of the temporary reg. will be overwritten
           there are 4 cases:
             I: a variable is currently in memory and its target is in physical reg
             II: a variable is currently in a register and its target is in memory
             III: a variable is currently in a different register
             IV: a variable is currently in a different memory location (for non-VRs)
           for GLUE, since it can only be allocated to %ebp, we don't have case III
           For now, case IV is not handled since it didn't show
        */
        if(compileTable[k].physicalReg != targetReg &&
           isVirtualReg(compileTable[k].physicalType)) {
            /* handles VR for case I to III */

            if(compileTable[k].physicalReg == PhysicalReg_Null && targetReg != PhysicalReg_Null) {
                /* handles VR for case I:
                   insert a xfer order from PhysicalReg_Null to targetReg */
                insertSrcReg(PhysicalReg_Null, targetReg, targetSpill, k);
#ifdef DEBUG_STATE
                ALOGI("insert for VR Null %d %d %d", targetReg, targetSpill, k);
#endif
            }

            if(compileTable[k].physicalReg != PhysicalReg_Null && targetReg != PhysicalReg_Null) {
                /* handles VR for case III
                   insert a xfer order from srcReg to targetReg */
                insertSrcReg(compileTable[k].physicalReg, targetReg, targetSpill, k);
            }

            if(compileTable[k].physicalReg != PhysicalReg_Null && targetReg == PhysicalReg_Null) {
                /* handles VR for case II
                   insert a xfer order from srcReg to memory */
                insertSrcReg(compileTable[k].physicalReg, targetReg, targetSpill, k);
            }
        }

        if(compileTable[k].physicalReg != targetReg &&
           !isVirtualReg(compileTable[k].physicalType)) {
            /* handles non-VR for case I to III */

            if(compileTable[k].physicalReg == PhysicalReg_Null && targetReg != PhysicalReg_Null) {
                /* handles non-VR for case I */
                if(compileTable[k].spill_loc_index < 0) {
                    /* this variable is freed, no need to transfer */
#ifdef DEBUG_STATE
                    ALOGW("in transferToState spill_loc_index is negative for temporary %d", compileTable[k].regNum);
#endif
                } else {
                    /* insert a xfer order from memory to targetReg */
#ifdef DEBUG_STATE
                    ALOGI("insert Null %d %d %d", targetReg, targetSpill, k);
#endif
                    insertSrcReg(PhysicalReg_Null, targetReg, targetSpill, k);
                }
            }

            if(compileTable[k].physicalReg != PhysicalReg_Null && targetReg != PhysicalReg_Null) {
                /* handles non-VR for case III
                   insert a xfer order from srcReg to targetReg */
                insertSrcReg(compileTable[k].physicalReg, targetReg, targetSpill, k);
            }

            if(compileTable[k].physicalReg != PhysicalReg_Null && targetReg == PhysicalReg_Null) {
                /* handles non-VR for case II */
                if(targetSpill < 0) {
                    /* this variable is freed, no need to transfer */
#ifdef DEBUG_STATE
                    ALOGW("in transferToState spill_loc_index is negative for temporary %d", compileTable[k].regNum);
#endif
                } else {
                    /* insert a xfer order from srcReg to memory */
                    insertSrcReg(compileTable[k].physicalReg, targetReg, targetSpill, k);
                }
            }

        }
    }//for compile entries

    int k2;
#ifdef DEBUG_STATE
    for(k = 0; k < num_src_regs; k++) {
        ALOGI("SRCREG %d: ", srcRegs[k].physicalReg);
        for(k2 = 0; k2 < srcRegs[k].num_dests; k2++) {
            int index = srcRegs[k].dsts[k2].compileIndex;
            ALOGI("[%d %d %d: %d %d %d] ", srcRegs[k].dsts[k2].targetReg,
                   srcRegs[k].dsts[k2].targetSpill, srcRegs[k].dsts[k2].compileIndex,
                   compileTable[index].regNum, compileTable[index].physicalType,
                   compileTable[index].spill_loc_index);
        }
        ALOGI("");
    }
#endif

    /* construct an order: xfers from srcReg first, then xfers from memory */
    int num_handled = 0;
    int num_in_order = 0;
    for(k = 0; k < num_src_regs; k++) {
        if(srcRegs[k].physicalReg == PhysicalReg_Null) {
            handledSrc[k] = true;
            num_handled++;
        } else {
            handledSrc[k] = false;
        }
    }
    while(num_handled < num_src_regs) {
        int prev_handled = num_handled;
        for(k = 0; k < num_src_regs; k++) {
            if(handledSrc[k]) continue;
            bool canHandleNow = true;
            for(k2 = 0; k2 < srcRegs[k].num_dests; k2++) {
                if(dstStillInUse(srcRegs[k].dsts[k2].targetReg)) {
                    canHandleNow = false;
                    break;
                }
            }
            if(canHandleNow) {
                handledSrc[k] = true;
                num_handled++;
                handledOrder[num_in_order] = k;
                num_in_order++;
            }
        } //for k
        if(num_handled == prev_handled) {
            ALOGE("no progress in selecting order");
            dvmAbort();
        }
    } //while
    for(k = 0; k < num_src_regs; k++) {
        if(srcRegs[k].physicalReg == PhysicalReg_Null) {
            handledOrder[num_in_order] = k;
            num_in_order++;
        }
    }
    if(num_in_order != num_src_regs) {
        ALOGE("num_in_order != num_src_regs");
        dvmAbort();
    }
#ifdef DEBUG_STATE
    ALOGI("ORDER: ");
    for(k = 0; k < num_src_regs; k++) {
        ALOGI("%d ", handledOrder[k]);
    }
    ALOGI("");
#endif
}
//! transfer the state of register allocator to a state specified in a state table

//!
void transferToState(int stateNum) {
    freeReg(false); //do not spill GL
    int k;
#ifdef DEBUG_STATE
    ALOGI("STATE: transfer to state %d", stateNum);
#endif
    if(stateNum > 4 || stateNum < 1) ALOGE("state table overflow");
    constructSrcRegs(stateNum);
    int k4, k3;
    for(k4 = 0; k4 < num_src_regs; k4++) {
        int k2 = handledOrder[k4]; //index to srcRegs
        for(k3 = 0; k3 < srcRegs[k2].num_dests; k3++) {
            k = srcRegs[k2].dsts[k3].compileIndex;
            int targetReg = srcRegs[k2].dsts[k3].targetReg;
            int targetSpill = srcRegs[k2].dsts[k3].targetSpill;
            if(compileTable[k].physicalReg != targetReg && isVirtualReg(compileTable[k].physicalType)) {
                OpndSize oSize = getRegSize(compileTable[k].physicalType);
                bool isSS = ((compileTable[k].physicalType & MASK_FOR_TYPE) == LowOpndRegType_ss);
                if(compileTable[k].physicalReg == PhysicalReg_Null && targetReg != PhysicalReg_Null) {
                    if(isSS)
                        move_ss_mem_to_reg_noalloc(4*compileTable[k].regNum,
                                                   PhysicalReg_FP, true,
                                                   MemoryAccess_VR, compileTable[k].regNum,
                                                   targetReg, true);
                    else
                        move_mem_to_reg_noalloc(oSize, 4*compileTable[k].regNum,
                                                PhysicalReg_FP, true,
                                                MemoryAccess_VR, compileTable[k].regNum,
                                                targetReg, true);
                }
                if(compileTable[k].physicalReg != PhysicalReg_Null && targetReg != PhysicalReg_Null) {
                    move_reg_to_reg_noalloc((isSS ? OpndSize_64 : oSize),
                                            compileTable[k].physicalReg, true,
                                            targetReg, true);
                }
                if(compileTable[k].physicalReg != PhysicalReg_Null && targetReg == PhysicalReg_Null) {
                    dumpToMem(compileTable[k].regNum, (LowOpndRegType)(compileTable[k].physicalType & MASK_FOR_TYPE),
                              compileTable[k].physicalReg);
                }
            } //VR
            if(compileTable[k].physicalReg != targetReg && !isVirtualReg(compileTable[k].physicalType)) {
                OpndSize oSize = getRegSize(compileTable[k].physicalType);
                if(compileTable[k].physicalReg == PhysicalReg_Null && targetReg != PhysicalReg_Null) {
                    loadFromSpillRegion(oSize, targetReg,
                                        compileTable[k].spill_loc_index);
                }
                //both are not null, move from one to the other
                if(compileTable[k].physicalReg != PhysicalReg_Null && targetReg != PhysicalReg_Null) {
                    move_reg_to_reg_noalloc(oSize, compileTable[k].physicalReg, true,
                                            targetReg, true);
                }
                //current is not null, target is null (move from reg to memory)
                if(compileTable[k].physicalReg != PhysicalReg_Null && targetReg == PhysicalReg_Null) {
                    saveToSpillRegion(oSize, compileTable[k].physicalReg, targetSpill);
                }
            } //temporary
        }//for
    }//for
    for(k = 0; k < num_memory_vr; k++) {
        bool targetBool = false;
        int targetReg = -1;
        if(stateNum == 1) {
            targetReg = stateTable2_1[k].regNum;
            targetBool = stateTable2_1[k].inMemory;
        }
        else if(stateNum == 2) {
            targetReg = stateTable2_2[k].regNum;
            targetBool = stateTable2_2[k].inMemory;
        }
        else if(stateNum == 3) {
            targetReg = stateTable2_3[k].regNum;
            targetBool = stateTable2_3[k].inMemory;
        }
        else if(stateNum == 4) {
            targetReg = stateTable2_4[k].regNum;
            targetBool = stateTable2_4[k].inMemory;
        }
        if(targetReg != memVRTable[k].regNum)
            ALOGE("regNum mismatch in transferToState");
        if(targetBool && (!memVRTable[k].inMemory)) {
            //dump to memory, check entries in compileTable: vA gp vA xmm vA ss
#ifdef DEBUG_STATE
            ALOGW("inMemory mismatch for VR %d in transferToState", targetReg);
#endif
            bool doneXfer = false;
            int index = searchCompileTable(LowOpndRegType_xmm | LowOpndRegType_virtual, targetReg);
            if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
                dumpToMem(targetReg, LowOpndRegType_xmm, compileTable[index].physicalReg);
                doneXfer = true;
            }
            if(!doneXfer) { //vA-1, xmm
                index = searchCompileTable(LowOpndRegType_xmm | LowOpndRegType_virtual, targetReg-1);
                if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
                    dumpToMem(targetReg-1, LowOpndRegType_xmm, compileTable[index].physicalReg);
                    doneXfer = true;
                }
            }
            if(!doneXfer) { //vA gp
                index = searchCompileTable(LowOpndRegType_gp | LowOpndRegType_virtual, targetReg);
                if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
                    dumpToMem(targetReg, LowOpndRegType_gp, compileTable[index].physicalReg);
                    doneXfer = true;
                }
            }
            if(!doneXfer) { //vA, ss
                index = searchCompileTable(LowOpndRegType_ss | LowOpndRegType_virtual, targetReg);
                if(index >= 0 && compileTable[index].physicalReg != PhysicalReg_Null) {
                    dumpToMem(targetReg, LowOpndRegType_ss, compileTable[index].physicalReg);
                    doneXfer = true;
                }
            }
            if(!doneXfer) ALOGW("can't match inMemory of VR %d in transferToState", targetReg);
        }
        if((!targetBool) && memVRTable[k].inMemory) {
            //do nothing
        }
    }
#ifdef DEBUG_STATE
    ALOGI("END transferToState %d", stateNum);
#endif
    goToState(stateNum);
}
/* END code to handle state transfers */
