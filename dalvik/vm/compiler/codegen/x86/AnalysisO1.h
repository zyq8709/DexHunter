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


/*! \file ncg_o1_data.h
    \brief A header file to define data structures used by register allocator & const folding
*/
#ifndef _DALVIK_NCG_ANALYSISO1_H
#define _DALVIK_NCG_ANALYSISO1_H

#include "Dalvik.h"
#include "enc_wrapper.h"
#include "Lower.h"
#ifdef WITH_JIT
#include "compiler/CompilerIR.h"
#endif

//! maximal number of edges per basic block
#define MAX_NUM_EDGE_PER_BB 300
//! maximal number of basic blocks per method
#define MAX_NUM_BBS_PER_METHOD 1000
//! maximal number of virtual registers per basic block
#define MAX_REG_PER_BASICBLOCK 140
//! maximal number of virtual registers per bytecode
#define MAX_REG_PER_BYTECODE 40
//! maximal number of virtual registers per method
#define MAX_REG_PER_METHOD 200
//! maximal number of temporaries per bytecode
#define MAX_TEMP_REG_PER_BYTECODE 30
//! maximal number of GG GPR VRs in a method
#define MAX_GLOBAL_VR      2
//! maximal number of GG XMM VRs in a method
#define MAX_GLOBAL_VR_XMM  4
#define MAX_CONST_REG 150

#define MASK_FOR_TYPE 7 //last 3 bits 111

#define LOOP_COUNT 10
//! maximal number of entries in compileTable
#define COMPILE_TABLE_SIZE 200
//! maximal number of transfer points per basic block
#define MAX_XFER_PER_BB 1000  //on Jan 4
#define PC_FOR_END_OF_BB -999
#define PC_FOR_START_OF_BB -998

//! various cases of overlapping between 2 variables
typedef enum OverlapCase {
  OVERLAP_ALIGN = 0,
  OVERLAP_B_IS_LOW_OF_A,
  OVERLAP_B_IS_HIGH_OF_A,
  OVERLAP_LOW_OF_A_IS_HIGH_OF_B,
  OVERLAP_HIGH_OF_A_IS_LOW_OF_B,
  OVERLAP_A_IS_LOW_OF_B,
  OVERLAP_A_IS_HIGH_OF_B,
  OVERLAP_B_COVER_A,
  OVERLAP_B_COVER_LOW_OF_A,
  OVERLAP_B_COVER_HIGH_OF_A,
  OVERLAP_NO
} OverlapCase;

//!access type of a variable
typedef enum RegAccessType {
  REGACCESS_D = 0,
  REGACCESS_U,
  REGACCESS_DU,
  REGACCESS_UD,
  REGACCESS_L,
  REGACCESS_H,
  REGACCESS_UL,
  REGACCESS_UH,
  REGACCESS_LU,
  REGACCESS_HU,
  REGACCESS_N, //no access
  REGACCESS_UNKNOWN
} RegAccessType;
//! a variable can be local (L), globally local (GL) or global (GG)
typedef enum GlobalType {
  GLOBALTYPE_GG,
  GLOBALTYPE_GL,
  GLOBALTYPE_L
} GlobalType;
typedef enum VRState {
  VRSTATE_SPILLED,
  VRSTATE_UPDATED,
  VRSTATE_CLEAN
} VRState;
//! helper state to determine if freeing VRs needs to be delayed
enum VRDelayFreeFlags {
  VRDELAY_NONE = 0, // used when VR can be freed from using physical register if needed
  VRDELAY_NULLCHECK = 1 << 0, // used when VR is used for null check and freeing must be delayed
  VRDELAY_BOUNDCHECK = 1 << 1 // used when VR is used for bound check and freeing must be delayed
};
typedef enum TRState { //state of temporary registers
  TRSTATE_SPILLED,
  TRSTATE_UNSPILLED,
  TRSTATE_CLEAN
} TRState;
//!information about a physical register
typedef struct RegisterInfo {
  PhysicalReg physicalReg;
  bool isUsed;
  bool isCalleeSaved;
  int freeTimeStamp;
} RegisterInfo;
typedef struct UniqueRegister {
  LowOpndRegType physicalType;
  int regNum;
  int numExposedUsage;
  PhysicalReg physicalReg;
} UniqueRegister;
//!specifies the weight of a VR allocated to a specific physical register
//!it is used for GPR VR only
typedef struct RegAllocConstraint {
  PhysicalReg physicalReg;
  int count;
} RegAllocConstraint;

typedef enum XferType {
  XFER_MEM_TO_XMM, //for usage
  XFER_DEF_TO_MEM, //def is gp
  XFER_DEF_TO_GP_MEM,
  XFER_DEF_TO_GP,
  XFER_DEF_IS_XMM //def is xmm
} XferType;
typedef struct XferPoint {
  int tableIndex; //generated from a def-use pair
  XferType xtype;
  int offsetPC;
  int regNum; //get or set VR at offsetPC
  LowOpndRegType physicalType;

  //if XFER_DEF_IS_XMM
  int vr_gpl; //a gp VR that uses the lower half of the def
  int vr_gph;
  bool dumpToXmm;
  bool dumpToMem;
} XferPoint;

//!for def: accessType means which part of the VR defined at offestPC is live now
//!for use: accessType means which part of the usage comes from the reachingDef
typedef struct DefOrUse {
  int offsetPC; //!the program point
  int regNum; //!access the virtual reg
  LowOpndRegType physicalType; //!xmm or gp or ss
  RegAccessType accessType; //!D, L, H, N
} DefOrUse;
//!a link list of DefOrUse
typedef struct DefOrUseLink {
  int offsetPC;
  int regNum; //access the virtual reg
  LowOpndRegType physicalType; //xmm or gp
  RegAccessType accessType; //D, L, H, N
  struct DefOrUseLink* next;
} DefOrUseLink;
//!pair of def and uses
typedef struct DefUsePair {
  DefOrUseLink* uses;
  DefOrUseLink* useTail;
  int num_uses;
  DefOrUse def;
  struct DefUsePair* next;
} DefUsePair;

//!information associated with a virtual register
//!the pair <regNum, physicalType> uniquely determines a variable
typedef struct VirtualRegInfo {
  int regNum;
  LowOpndRegType physicalType;
  int refCount;
  RegAccessType accessType;
  GlobalType gType;
  int physicalReg_GG;
  RegAllocConstraint allocConstraints[8];
  RegAllocConstraint allocConstraintsSorted[8];

  DefOrUse reachingDefs[3]; //!reaching defs to the virtual register
  int num_reaching_defs;
} VirtualRegInfo;
//!information of whether a VR is constant and its value
typedef struct ConstVRInfo {
  int regNum;
  int value;
  bool isConst;
} ConstVRInfo;
#define NUM_ACCESS_IN_LIVERANGE 10
//!specifies one live range
typedef struct LiveRange {
  int start;
  int end; //inclusive
  //all accesses in the live range
  int num_access;
  int num_alloc;
  int* accessPC;
  struct LiveRange* next;
} LiveRange;
typedef struct BoundCheckIndex {
  int indexVR;
  bool checkDone;
} BoundCheckIndex;
//!information for a virtual register such as live ranges, in memory
typedef struct MemoryVRInfo {
  int regNum;
  bool inMemory;
  bool nullCheckDone;
  BoundCheckIndex boundCheck;
  int num_ranges;
  LiveRange* ranges;
  u4 delayFreeFlags; //! for use with flags defined by VRDelayFreeFlags enum
} MemoryVRInfo;
//!information of a temporary
//!the pair <regNum, physicalType> uniquely determines a variable
typedef struct TempRegInfo {
  int regNum;
  int physicalType;
  int refCount;
  int linkageToVR;
  int versionNum;
  bool shareWithVR; //for temp. regs updated by get_virtual_reg
  bool is8Bit;
} TempRegInfo;
struct BasicBlock_O1;
//!all variables accessed
//!the pair <regNum, physicalType> uniquely determines a variable
typedef struct compileTableEntry {
  int regNum;
  int physicalType; //gp, xmm or scratch, virtual
  int physicalReg;
  int physicalReg_prev; //for spilled GG VR
  RegAccessType accessType;

  bool isConst;
  int value[2]; //[0]: lower [1]: higher
  int refCount;

  int linkageToVR; //for temporary registers only
  GlobalType gType;
  struct BasicBlock_O1* bb; //bb VR belongs to
  int indexToInfoBB;

  VRState regState;
  TRState trState; //for temporary registers only
  int spill_loc_index; //for temporary registers only
} compileTableEntry;
//!to save the state of register allocator
typedef struct regAllocStateEntry1 {
  int spill_loc_index;
  int physicalReg;
} regAllocStateEntry1;
typedef struct regAllocStateEntry2 {
  int regNum; //
  bool inMemory; //whether 4-byte virtual reg is in memory
} regAllocStateEntry2;
//!edge in control flow graph
typedef struct Edge_O1 {
  struct BasicBlock_O1* src;
  struct BasicBlock_O1* dst;
} Edge_O1;
//!information associated with a basic block
typedef struct BasicBlock_O1 {
  int bb_index;
  int bb_index2;
  int pc_start;       //!inclusive
#ifndef WITH_JIT
  int pc_end;         //!exclusive
  Edge_O1* in_edges[MAX_NUM_EDGE_PER_BB]; //array of Edge*
  int num_in_edges;
  Edge_O1* out_edges[MAX_NUM_EDGE_PER_BB];
  int num_out_edges;
#else
  int pc_end;
  BasicBlock* jitBasicBlock;
#endif
  VirtualRegInfo infoBasicBlock[MAX_REG_PER_BASICBLOCK];
  int num_regs;

  RegAllocConstraint allocConstraints[8]; //# of times a hardcoded register is used in this basic block
  //a physical register that is used many times has a lower priority to get picked in getFreeReg
  RegAllocConstraint allocConstraintsSorted[8]; //count from low to high

  DefUsePair* defUseTable;
  DefUsePair* defUseTail;
  int num_defs;
  XferPoint xferPoints[MAX_XFER_PER_BB]; //program points where the transfer is required
  int num_xfer_points;

  bool endsWithReturn;
  bool hasAccessToGlue;
} BasicBlock_O1;
typedef struct CFG_O1 {
  BasicBlock_O1* head;
} CFG_O1;
//!worklist to create a control flow graph
typedef struct CFGWork {
  BasicBlock_O1* bb_prev;
  int targetOff;
  struct CFGWork* nextItem;
} CFGWork;

/////////////////////////////////////////
extern compileTableEntry compileTable[COMPILE_TABLE_SIZE];
extern int num_compile_entries;
extern VirtualRegInfo infoByteCode[MAX_REG_PER_BYTECODE];
extern int num_regs_per_bytecode;
extern TempRegInfo infoByteCodeTemp[MAX_TEMP_REG_PER_BYTECODE];
extern int num_temp_regs_per_bytecode;
extern VirtualRegInfo infoMethod[MAX_REG_PER_METHOD];
extern int num_regs_per_method;
extern BasicBlock_O1* currentBB;

extern BasicBlock_O1* method_bbs[MAX_NUM_BBS_PER_METHOD];
extern int num_bbs_for_method;
extern BasicBlock_O1* method_bbs_sorted[MAX_NUM_BBS_PER_METHOD];
extern BasicBlock_O1* bb_entry;
extern int pc_start;
extern int pc_end;
extern int current_bc_size;
extern int num_exception_handlers;
extern int exceptionHandlers[10];

extern int num_const_vr;
extern ConstVRInfo constVRTable[MAX_CONST_REG];

extern int genSet[MAX_REG_PER_BYTECODE];
extern int killSet[MAX_REG_PER_BYTECODE];
extern int num_regs_gen; //per bytecode
extern int num_regs_kill; //per bytecode

extern int genSetBB[MAX_NUM_BBS_PER_METHOD][40];
extern int killSetBB[MAX_NUM_BBS_PER_METHOD][40]; //same as size of memVRTable
extern int num_gen_bb[MAX_NUM_BBS_PER_METHOD];
extern int num_kill_bb[MAX_NUM_BBS_PER_METHOD];

extern int nullCheck_inB[MAX_NUM_BBS_PER_METHOD][40];
extern int nullCheck_inSize[MAX_NUM_BBS_PER_METHOD];
extern int nullCheck_outB[MAX_NUM_BBS_PER_METHOD][40];
extern int nullCheck_outSize[MAX_NUM_BBS_PER_METHOD];

typedef enum GlueVarType {
  RES_CLASS = 0,
  RES_METHOD,
  RES_FIELD,
  RES_STRING,
  GLUE_DVMDEX,
  GLUE_METHOD_CLASS,
  GLUE_METHOD
} GlueVarType;

void forwardAnalysis(int type);

//functions in bc_visitor.c
int getByteCodeSize();
bool getConstInfo(BasicBlock_O1* bb);
int getVirtualRegInfo(VirtualRegInfo* infoArray);
int getTempRegInfo(TempRegInfo* infoArray);
int createCFGHandler(Method* method);

int findVirtualRegInTable(u2 vA, LowOpndRegType type, bool printError);
int searchCompileTable(int type, int regNum);
BasicBlock_O1* createBasicBlock(int src_pc, int end_pc);
void handleJump(BasicBlock_O1* bb_prev, int relOff);
void connectBasicBlock(BasicBlock_O1* src, BasicBlock_O1* dst);
int insertWorklist(BasicBlock_O1* bb_prev, int targetOff);

int collectInfoOfBasicBlock(Method* method, BasicBlock_O1* bb); //update bb->infoBasicBlock

void updateCurrentBBWithConstraints(PhysicalReg reg);
void updateConstInfo(BasicBlock_O1*);
OpndSize getRegSize(int type);
void invalidateVRDueToConst(int reg, OpndSize size);
#endif

