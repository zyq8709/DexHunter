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


/*! \file lower.h
    \brief A header file to define interface between lowering and register allocator
*/

#ifndef _DALVIK_LOWER
#define _DALVIK_LOWER

#define CODE_CACHE_PADDING 1024 //code space for a single bytecode
// comment out for phase 1 porting
#define PREDICTED_CHAINING
#define JIT_CHAIN

#define NUM_DEPENDENCIES 24 /* max number of dependencies from a LowOp */
//compilaton flags used by NCG O1
#define DUMP_EXCEPTION //to measure performance, required to have correct exception handling
/*! multiple versions for hardcoded registers */
#define HARDREG_OPT
#define CFG_OPT
/*! remove redundant move ops when accessing virtual registers */
#define MOVE_OPT
/*! remove redundant spill of virtual registers */
#define SPILL_OPT
#define XFER_OPT
//#define DSE_OPT //no perf improvement for cme
/*! use live range analysis to allocate registers */
#define LIVERANGE_OPT
/*! remove redundant null check */
#define NULLCHECK_OPT
//#define BOUNDCHECK_OPT
/*! optimize the access to glue structure */
#define GLUE_OPT
#define CALL_FIX
#define NATIVE_FIX
#define INVOKE_FIX //optimization
#define GETVR_FIX //optimization

#include "Dalvik.h"
#include "enc_wrapper.h"
#include "AnalysisO1.h"
#include "compiler/CompilerIR.h"

//compilation flags for debugging
//#define DEBUG_INFO
//#define DEBUG_CALL_STACK
//#define DEBUG_IGET_OBJ
//#define DEBUG_NCG_CODE_SIZE
//#define DEBUG_NCG
//#define DEBUG_NCG_1
//#define DEBUG_LOADING
//#define USE_INTERPRETER
//#define DEBUG_EACH_BYTECODE

/*! registers for functions are hardcoded */
#define HARDCODE_REG_CALL
#define HARDCODE_REG_SHARE
#define HARDCODE_REG_HELPER

#define PhysicalReg_FP PhysicalReg_EDI
#define PhysicalReg_Glue PhysicalReg_EBP

//COPIED from interp/InterpDefs.h
#define FETCH(_offset) (rPC[(_offset)])
#define INST_INST(_inst) ((_inst) & 0xff)
#define INST_A(_inst)       (((_inst) >> 8) & 0x0f)
#define INST_B(_inst)       ((_inst) >> 12)
#define INST_AA(_inst)      ((_inst) >> 8)

//#include "vm/mterp/common/asm-constants.h"
#define offEBP_self 8
#define offEBP_spill -56
#define offThread_exception 68
#define offClassObject_descriptor 24
#define offArrayObject_length 8
#ifdef PROFILE_FIELD_ACCESS
#define offStaticField_value 24
#define offInstField_byteOffset 24
#else
#define offStaticField_value 16
#define offInstField_byteOffset 16
#endif

#ifdef EASY_GDB
#define offStackSaveArea_prevFrame 4
#define offStackSaveArea_savedPc 8
#define offStackSaveArea_method 12
#define offStackSaveArea_localRefTop 16 // -> StackSaveArea.xtra.locakRefCookie
#define offStackSaveArea_returnAddr 20
#define offStackSaveArea_isDebugInterpreted 24
#define sizeofStackSaveArea 24
#else
#define offStackSaveArea_prevFrame 0
#define offStackSaveArea_savedPc 4
#define offStackSaveArea_method 8
#define offStackSaveArea_localRefTop 12 // -> StackSaveArea.xtra.locakRefCookie
#define offStackSaveArea_returnAddr 16
#define offStackSaveArea_isDebugInterpreted 20
#define sizeofStackSaveArea 20
#endif

#define offClassObject_status 44
#define offClassObject_accessFlags 32
#ifdef MTERP_NO_UNALIGN_64
#define offArrayObject_contents 16
#else
#define offArrayObject_contents 12
#endif

#define offField_clazz 0
#define offObject_clazz 0
#define offClassObject_vtable 116
#define offClassObject_pDvmDex 40
#define offClassObject_super 72
#define offClassObject_vtableCount 112
#define offMethod_name 16
#define offMethod_accessFlags 4
#define offMethod_methodIndex 8
#define offMethod_registersSize 10
#define offMethod_outsSize 12
#define offGlue_interpStackEnd 32
#define offThread_inJitCodeCache 124
#define offThread_jniLocal_nextEntry 168
#define offMethod_insns 32
#ifdef ENABLE_TRACING
#define offMethod_insns_bytecode 44
#define offMethod_insns_ncg 48
#endif

#define offGlue_pc     0
#define offGlue_fp     4
#define offGlue_retval 8

#define offThread_curFrame 4
#define offGlue_method 16
#define offGlue_methodClassDex 20
#define offGlue_self 24
#define offGlue_pSelfSuspendCount 36
#define offGlue_cardTable 40
#define offGlue_pDebuggerActive 44
#define offGlue_pActiveProfilers 48
#define offGlue_entryPoint 52
#define offGlue_icRechainCount 84
#define offGlue_espEntry 88
#define offGlue_spillRegion 92
#define offDvmDex_pResStrings 8
#define offDvmDex_pResClasses 12
#define offDvmDex_pResMethods 16
#define offDvmDex_pResFields  20
#define offMethod_clazz       0

// Definitions must be consistent with vm/mterp/x86/header.S
#define FRAME_SIZE     124

typedef enum ArgsDoneType {
    ArgsDone_Normal = 0,
    ArgsDone_Native,
    ArgsDone_Full
} ArgsDoneType;

/*! An enum type
    to list bytecodes for AGET, APUT
*/
typedef enum ArrayAccess {
    AGET, AGET_WIDE, AGET_CHAR, AGET_SHORT, AGET_BOOLEAN, AGET_BYTE,
    APUT, APUT_WIDE, APUT_CHAR, APUT_SHORT, APUT_BOOLEAN, APUT_BYTE
} ArrayAccess;
/*! An enum type
    to list bytecodes for IGET, IPUT
*/
typedef enum InstanceAccess {
    IGET, IGET_WIDE, IPUT, IPUT_WIDE
} InstanceAccess;
/*! An enum type
    to list bytecodes for SGET, SPUT
*/
typedef enum StaticAccess {
    SGET, SGET_WIDE, SPUT, SPUT_WIDE
} StaticAccess;

typedef enum JmpCall_type {
    JmpCall_uncond = 1,
    JmpCall_cond,
    JmpCall_reg, //jump reg32
    JmpCall_call
} JmpCall_type;

////////////////////////////////////////////////////////////////
/* data structure for native codes */
/* Due to space considation, a lowered op (LowOp) has two operands (LowOpnd), depending on
   the type of the operand, LowOpndReg or LowOpndImm or LowOpndMem will follow */
/*! type of an operand can be immediate, register or memory */
typedef enum LowOpndType {
  LowOpndType_Imm = 0,
  LowOpndType_Reg,
  LowOpndType_Mem,
  LowOpndType_Label,
  LowOpndType_NCG,
  LowOpndType_Chain
} LowOpndType;
typedef enum LowOpndDefUse {
  LowOpndDefUse_Def = 0,
  LowOpndDefUse_Use,
  LowOpndDefUse_UseDef
} LowOpndDefUse;

/*!
\brief base data structure for an operand */
typedef struct LowOpnd {
  LowOpndType type;
  OpndSize size;
  LowOpndDefUse defuse;
} LowOpnd;
/*!
\brief data structure for a register operand */
typedef struct LowOpndReg {
  LowOpndRegType regType;
  int logicalReg;
  int physicalReg;
} LowOpndReg;
/*!
\brief data structure for an immediate operand */
typedef struct LowOpndImm {
  union {
    s4 value;
    unsigned char bytes[4];
  };
} LowOpndImm;

typedef struct LowOpndNCG {
  union {
    s4 value;
    unsigned char bytes[4];
  };
} LowOpndNCG;

#define LABEL_SIZE 256
typedef struct LowOpndLabel {
  char label[LABEL_SIZE];
  bool isLocal;
} LowOpndLabel;

/* get ready for optimizations at LIR
   add MemoryAccessType & virtualRegNum to memory operands */
typedef enum MemoryAccessType {
  MemoryAccess_GLUE,
  MemoryAccess_VR,
  MemoryAccess_SPILL,
  MemoryAccess_Unknown
} MemoryAccessType;
typedef enum UseDefEntryType {
  UseDefType_Ctrl = 0,
  UseDefType_Float,
  UseDefType_MemVR,
  UseDefType_MemSpill,
  UseDefType_MemUnknown,
  UseDefType_Reg
} UseDefEntryType;
typedef struct UseDefProducerEntry {
  UseDefEntryType type;
  int index; //enum PhysicalReg for "Reg" type
  int producerSlot;
} UseDefProducerEntry;
#define MAX_USE_PER_ENTRY 50 /* at most 10 uses for each entry */
typedef struct UseDefUserEntry {
  UseDefEntryType type;
  int index;
  int useSlots[MAX_USE_PER_ENTRY];
  int num_uses_per_entry;
} UseDefUserEntry;

/*!
\brief data structure for a memory operand */
typedef struct LowOpndMem {
  LowOpndImm m_disp;
  LowOpndImm m_scale;
  LowOpndReg m_index;
  LowOpndReg m_base;
  bool hasScale;
  MemoryAccessType mType;
  int index;
} LowOpndMem;

typedef enum AtomOpCode {
    ATOM_PSEUDO_CHAINING_CELL_BACKWARD_BRANCH = -15,
    ATOM_NORMAL_ALU = -14,
    ATOM_PSEUDO_ENTRY_BLOCK = -13,
    ATOM_PSEUDO_EXIT_BLOCK = -12,
    ATOM_PSEUDO_TARGET_LABEL = -11,
    ATOM_PSEUDO_CHAINING_CELL_HOT = -10,
    ATOM_PSEUDO_CHAINING_CELL_INVOKE_PREDICTED = -9,
    ATOM_PSEUDO_CHAINING_CELL_INVOKE_SINGLETON = -8,
    ATOM_PSEUDO_CHAINING_CELL_NORMAL = -7,
    ATOM_PSEUDO_DALVIK_BYTECODE_BOUNDARY = -6,
    ATOM_PSEUDO_ALIGN4 = -5,
    ATOM_PSEUDO_PC_RECONSTRUCTION_CELL = -4,
    ATOM_PSEUDO_PC_RECONSTRUCTION_BLOCK_LABEL = -3,
    ATOM_PSEUDO_EH_BLOCK_LABEL = -2,
    ATOM_PSEUDO_NORMAL_BLOCK_LABEL = -1,
    ATOM_NORMAL,
} AtomOpCode;

typedef enum DependencyType {
  Dependency_RAW,
  Dependency_WAW,
  Dependency_WAR,
  Dependency_FLAG
} DependencyType;
typedef struct DependencyStruct {
  DependencyType dType;
  int nodeId;
  int latency;
} DependencyStruct;

typedef struct LowOpBlock {
  LIR generic;
  Mnemonic opCode;
  AtomOpCode opCode2;
} LowOpBlock;

/*!
\brief data structure for a lowered operation */
typedef struct LowOp {
  LIR generic;
  Mnemonic opCode;
  AtomOpCode opCode2;
  LowOpnd opnd1;
  LowOpnd opnd2;
  int numOperands;
} LowOp;

typedef struct LowOpLabel {
  LowOp lop;
  LowOpndLabel labelOpnd;
}LowOpLabel;

typedef struct LowOpNCG {
  LowOp lop;
  LowOpndNCG ncgOpnd;
}LowOpNCG;

typedef struct LowOpBlockLabel {
  LowOpBlock lop;
  LowOpndImm immOpnd;
} LowOpBlockLabel;

typedef struct LowOpImm {
  LowOp lop;
  LowOpndImm immOpnd;
} LowOpImm;

typedef struct LowOpMem {
  LowOp lop;
  LowOpndMem memOpnd;
} LowOpMem;

typedef struct LowOpReg {
  LowOp lop;
  LowOpndReg regOpnd;
} LowOpReg;

typedef struct LowOpImmImm {
  LowOp lop;
  LowOpndImm immOpnd1;
  LowOpndImm immOpnd2;
} LowOpImmImm;

typedef struct LowOpImmReg {
  LowOp lop;
  LowOpndImm immOpnd1;
  LowOpndReg regOpnd2;
} LowOpImmReg;

typedef struct LowOpImmMem {
  LowOp lop;
  LowOpndImm immOpnd1;
  LowOpndMem memOpnd2;
} LowOpImmMem;

typedef struct LowOpRegImm {
  LowOp lop;
  LowOpndReg regOpnd1;
  LowOpndImm immOpnd2;
} LowOpRegImm;

typedef struct LowOpRegReg {
  LowOp lop;
  LowOpndReg regOpnd1;
  LowOpndReg regOpnd2;
} LowOpRegReg;

typedef struct LowOpRegMem {
  LowOp lop;
  LowOpndReg regOpnd1;
  LowOpndMem memOpnd2;
} LowOpRegMem;

typedef struct LowOpMemImm {
  LowOp lop;
  LowOpndMem memOpnd1;
  LowOpndImm immOpnd2;
} LowOpMemImm;

typedef struct LowOpMemReg {
  LowOp lop;
  LowOpndMem memOpnd1;
  LowOpndReg regOpnd2;
} LowOpMemReg;

typedef struct LowOpMemMem {
  LowOp lop;
  LowOpndMem memOpnd1;
  LowOpndMem memOpnd2;
} LowOpMemMem;

/*!
\brief data structure for labels used when lowering a method

four label maps are defined: globalMap globalShortMap globalWorklist globalShortWorklist
globalMap: global labels where codePtr points to the label
           freeLabelMap called in clearNCG
globalWorklist: global labels where codePtr points to an instruciton using the label
  standalone NCG -------
                accessed by insertLabelWorklist & performLabelWorklist
  code cache ------
                inserted by performLabelWorklist(false),
                handled & cleared by generateRelocation in NcgFile.c
globalShortMap: local labels where codePtr points to the label
                freeShortMap called after generation of one bytecode
globalShortWorklist: local labels where codePtr points to an instruction using the label
                accessed by insertShortWorklist & insertLabel
definition of local label: life time of the label is within a bytecode or within a helper function
extra label maps are used by code cache:
  globalDataWorklist VMAPIWorklist
*/
typedef struct LabelMap {
  char label[LABEL_SIZE];
  char* codePtr; //code corresponding to the label or code that uses the label
  struct LabelMap* nextItem;
  OpndSize size;
  uint  addend;
} LabelMap;
/*!
\brief data structure to handle forward jump (GOTO, IF)

accessed by insertNCGWorklist & performNCGWorklist
*/
typedef struct NCGWorklist {
  //when WITH_JIT, relativePC stores the target basic block id
  s4 relativePC; //relative offset in bytecode
  int offsetPC;  //PC in bytecode
  int offsetNCG; //PC in native code
  char* codePtr; //code for native jump instruction
  struct NCGWorklist* nextItem;
  OpndSize size;
}NCGWorklist;
/*!
\brief data structure to handle SWITCH & FILL_ARRAY_DATA

two data worklist are defined: globalDataWorklist (used by code cache) & methodDataWorklist
methodDataWorklist is accessed by insertDataWorklist & performDataWorklist
*/
typedef struct DataWorklist {
  s4 relativePC; //relative offset in bytecode to access the data
  int offsetPC;  //PC in bytecode
  int offsetNCG; //PC in native code
  char* codePtr; //code for native instruction add_imm_reg imm, %edx
  char* codePtr2;//code for native instruction add_reg_reg %eax, %edx for SWITCH
                 //                            add_imm_reg imm, %edx for FILL_ARRAY_DATA
  struct DataWorklist* nextItem;
}DataWorklist;
#ifdef ENABLE_TRACING
typedef struct MapWorklist {
  u4 offsetPC;
  u4 offsetNCG;
  int isStartOfPC; //1 --> true 0 --> false
  struct MapWorklist* nextItem;
} MapWorklist;
#endif

#define BUFFER_SIZE 1024 //# of Low Ops buffered
//the following three numbers are hardcoded, please CHECK
#define BYTECODE_SIZE_PER_METHOD 81920
#define NATIVE_SIZE_PER_DEX 19000000 //FIXME for core.jar: 16M --> 18M for O1
#define NATIVE_SIZE_FOR_VM_STUBS 100000
#define MAX_HANDLER_OFFSET 1024 //maximal number of handler offsets

extern int LstrClassCastExceptionPtr, LstrInstantiationErrorPtr, LstrInternalError, LstrFilledNewArrayNotImpl;
extern int LstrArithmeticException, LstrArrayIndexException, LstrArrayStoreException, LstrStringIndexOutOfBoundsException;
extern int LstrDivideByZero, LstrNegativeArraySizeException, LstrNoSuchMethodError, LstrNullPointerException;
extern int LdoubNeg, LvaluePosInfLong, LvalueNegInfLong, LvalueNanLong, LshiftMask, Lvalue64, L64bits, LintMax, LintMin;

extern LabelMap* globalMap;
extern LabelMap* globalShortMap;
extern LabelMap* globalWorklist;
extern LabelMap* globalShortWorklist;
extern NCGWorklist* globalNCGWorklist;
extern DataWorklist* methodDataWorklist;
#ifdef ENABLE_TRACING
extern MapWorklist* methodMapWorklist;
#endif
extern PhysicalReg scratchRegs[4];

#define C_SCRATCH_1 scratchRegs[0]
#define C_SCRATCH_2 scratchRegs[1]
#define C_SCRATCH_3 scratchRegs[2] //scratch reg inside callee

extern LowOp* ops[BUFFER_SIZE];
extern bool isScratchPhysical;
extern u2* rPC;
extern u2 inst;
extern int offsetPC;
extern int offsetNCG;
extern int mapFromBCtoNCG[BYTECODE_SIZE_PER_METHOD];
extern char* streamStart;

extern char* streamCode;

extern char* streamMethodStart; //start of the method
extern char* stream; //current stream pointer
extern char* streamMisPred;
extern int lowOpTimeStamp;
extern Method* currentMethod;
extern int currentExceptionBlockIdx;

extern int globalMapNum;
extern int globalWorklistNum;
extern int globalDataWorklistNum;
extern int globalPCWorklistNum;
extern int chainingWorklistNum;
extern int VMAPIWorklistNum;

extern LabelMap* globalDataWorklist;
extern LabelMap* globalPCWorklist;
extern LabelMap* chainingWorklist;
extern LabelMap* VMAPIWorklist;

extern int ncgClassNum;
extern int ncgMethodNum;

extern LowOp* lirTable[200]; //Number of LIRs for all bytecodes do not exceed 200
extern int num_lirs_in_table;

bool existATryBlock(Method* method, int startPC, int endPC);
// interface between register allocator & lowering
extern int num_removed_nullCheck;

int registerAlloc(int type, int reg, bool isPhysical, bool updateRef);
int registerAllocMove(int reg, int type, bool isPhysical, int srcReg);
int checkVirtualReg(int reg, LowOpndRegType type, int updateRef); //returns the physical register
int updateRefCount(int reg, LowOpndRegType type);
int updateRefCount2(int reg, int type, bool isPhysical);
int spillVirtualReg(int vrNum, LowOpndRegType type, bool updateTable);
int isVirtualRegConstant(int regNum, LowOpndRegType type, int* valuePtr, bool updateRef);
int checkTempReg(int reg, int type, bool isPhysical, int vA);
bool checkTempReg2(int reg, int type, bool isPhysical, int physicalRegForVR);
int freeReg(bool spillGL);
int nextVersionOfHardReg(PhysicalReg pReg, int refCount);
int updateVirtualReg(int reg, LowOpndRegType type);
void setVRNullCheck(int regNum, OpndSize size);
bool isVRNullCheck(int regNum, OpndSize size);
void setVRBoundCheck(int vr_array, int vr_index);
bool isVRBoundCheck(int vr_array, int vr_index);
int requestVRFreeDelay(int regNum, u4 reason);
void cancelVRFreeDelayRequest(int regNum, u4 reason);
bool getVRFreeDelayRequested(int regNum);
bool isGlueHandled(int glue_reg);
void resetGlue(int glue_reg);
void updateGlue(int reg, bool isPhysical, int glue_reg);
int updateVRAtUse(int reg, LowOpndRegType pType, int regAll);
int touchEcx();
int touchEax();
int touchEdx();
int beforeCall(const char* target);
int afterCall(const char* target);
void startBranch();
void endBranch();
void rememberState(int);
void goToState(int);
void transferToState(int);
void globalVREndOfBB(const Method*);
void constVREndOfBB();
void startNativeCode(int num, int type);
void endNativeCode();
void donotSpillReg(int physicalReg);
void doSpillReg(int physicalReg);

#define XMM_1 PhysicalReg_XMM0
#define XMM_2 PhysicalReg_XMM1
#define XMM_3 PhysicalReg_XMM2
#define XMM_4 PhysicalReg_XMM3

/////////////////////////////////////////////////////////////////////////////////
//LR[reg] = disp + PR[base_reg] or disp + LR[base_reg]
void load_effective_addr(int disp, int base_reg, bool isBasePhysical,
                          int reg, bool isPhysical);
void load_effective_addr_scale(int base_reg, bool isBasePhysical,
                                int index_reg, bool isIndexPhysical, int scale,
                                int reg, bool isPhysical);
void load_fpu_cw(int disp, int base_reg, bool isBasePhysical);
void store_fpu_cw(bool checkException, int disp, int base_reg, bool isBasePhysical);
void convert_integer(OpndSize srcSize, OpndSize dstSize);
void load_fp_stack(LowOp* op, OpndSize size, int disp, int base_reg, bool isBasePhysical);
void load_int_fp_stack(OpndSize size, int disp, int base_reg, bool isBasePhysical);
void load_int_fp_stack_imm(OpndSize size, int imm);
void store_fp_stack(LowOp* op, bool pop, OpndSize size, int disp, int base_reg, bool isBasePhysical);
void store_int_fp_stack(LowOp* op, bool pop, OpndSize size, int disp, int base_reg, bool isBasePhysical);

void load_fp_stack_VR(OpndSize size, int vA);
void load_int_fp_stack_VR(OpndSize size, int vA);
void store_fp_stack_VR(bool pop, OpndSize size, int vA);
void store_int_fp_stack_VR(bool pop, OpndSize size, int vA);
void compare_VR_ss_reg(int vA, int reg, bool isPhysical);
void compare_VR_sd_reg(int vA, int reg, bool isPhysical);
void fpu_VR(ALU_Opcode opc, OpndSize size, int vA);
void compare_reg_mem(LowOp* op, OpndSize size, int reg, bool isPhysical,
                           int disp, int base_reg, bool isBasePhysical);
void compare_mem_reg(OpndSize size,
                           int disp, int base_reg, bool isBasePhysical,
                           int reg, bool isPhysical);
void compare_VR_reg(OpndSize size,
                           int vA,
                           int reg, bool isPhysical);
void compare_imm_reg(OpndSize size, int imm,
                           int reg, bool isPhysical);
void compare_imm_mem(OpndSize size, int imm,
                           int disp, int base_reg, bool isBasePhysical);
void compare_imm_VR(OpndSize size, int imm,
                           int vA);
void compare_reg_reg(int reg1, bool isPhysical1,
                           int reg2, bool isPhysical2);
void compare_reg_reg_16(int reg1, bool isPhysical1,
                         int reg2, bool isPhysical2);
void compare_ss_mem_reg(LowOp* op, int disp, int base_reg, bool isBasePhysical,
                              int reg, bool isPhysical);
void compare_ss_reg_with_reg(LowOp* op, int reg1, bool isPhysical1,
                              int reg2, bool isPhysical2);
void compare_sd_mem_with_reg(LowOp* op, int disp, int base_reg, bool isBasePhysical,
                              int reg, bool isPhysical);
void compare_sd_reg_with_reg(LowOp* op, int reg1, bool isPhysical1,
                              int reg2, bool isPhysical2);
void compare_fp_stack(bool pop, int reg, bool isDouble);
void test_imm_reg(OpndSize size, int imm, int reg, bool isPhysical);
void test_imm_mem(OpndSize size, int imm, int disp, int reg, bool isPhysical);

void conditional_move_reg_to_reg(OpndSize size, ConditionCode cc, int reg1, bool isPhysical1, int reg, bool isPhysical);
void move_ss_mem_to_reg(LowOp* op, int disp, int base_reg, bool isBasePhysical,
                        int reg, bool isPhysical);
void move_ss_reg_to_mem(LowOp* op, int reg, bool isPhysical,
                         int disp, int base_reg, bool isBasePhysical);
LowOpRegMem* move_ss_mem_to_reg_noalloc(int disp, int base_reg, bool isBasePhysical,
                         MemoryAccessType mType, int mIndex,
                         int reg, bool isPhysical);
LowOpMemReg* move_ss_reg_to_mem_noalloc(int reg, bool isPhysical,
                         int disp, int base_reg, bool isBasePhysical,
                         MemoryAccessType mType, int mIndex);
void move_sd_mem_to_reg(int disp, int base_reg, bool isBasePhysical,
                         int reg, bool isPhysical);
void move_sd_reg_to_mem(LowOp* op, int reg, bool isPhysical,
                         int disp, int base_reg, bool isBasePhysical);

void conditional_jump(ConditionCode cc, const char* target, bool isShortTerm);
void unconditional_jump(const char* target, bool isShortTerm);
void conditional_jump_int(ConditionCode cc, int target, OpndSize size);
void unconditional_jump_int(int target, OpndSize size);
void unconditional_jump_reg(int reg, bool isPhysical);
void call(const char* target);
void call_reg(int reg, bool isPhysical);
void call_reg_noalloc(int reg, bool isPhysical);
void call_mem(int disp, int reg, bool isPhysical);
void x86_return();

void alu_unary_reg(OpndSize size, ALU_Opcode opc, int reg, bool isPhysical);
void alu_unary_mem(LowOp* op, OpndSize size, ALU_Opcode opc, int disp, int base_reg, bool isBasePhysical);

void alu_binary_imm_mem(OpndSize size, ALU_Opcode opc,
                         int imm, int disp, int base_reg, bool isBasePhysical);
void alu_binary_imm_reg(OpndSize size, ALU_Opcode opc, int imm, int reg, bool isPhysical);
void alu_binary_mem_reg(OpndSize size, ALU_Opcode opc,
                         int disp, int base_reg, bool isBasePhysical,
                         int reg, bool isPhysical);
void alu_binary_VR_reg(OpndSize size, ALU_Opcode opc, int vA, int reg, bool isPhysical);
void alu_sd_binary_VR_reg(ALU_Opcode opc, int vA, int reg, bool isPhysical, bool isSD);
void alu_binary_reg_reg(OpndSize size, ALU_Opcode opc,
                         int reg1, bool isPhysical1,
                         int reg2, bool isPhysical2);
void alu_binary_reg_mem(OpndSize size, ALU_Opcode opc,
                         int reg, bool isPhysical,
                         int disp, int base_reg, bool isBasePhysical);

void fpu_mem(LowOp* op, ALU_Opcode opc, OpndSize size, int disp, int base_reg, bool isBasePhysical);
void alu_ss_binary_reg_reg(ALU_Opcode opc, int reg, bool isPhysical,
                            int reg2, bool isPhysical2);
void alu_sd_binary_reg_reg(ALU_Opcode opc, int reg, bool isPhysical,
                            int reg2, bool isPhysical2);

void push_mem_to_stack(OpndSize size, int disp, int base_reg, bool isBasePhysical);
void push_reg_to_stack(OpndSize size, int reg, bool isPhysical);

//returns the pointer to end of the native code
void move_reg_to_mem(OpndSize size,
                      int reg, bool isPhysical,
                      int disp, int base_reg, bool isBasePhysical);
LowOpRegMem* move_mem_to_reg(OpndSize size,
                      int disp, int base_reg, bool isBasePhysical,
                      int reg, bool isPhysical);
void movez_mem_to_reg(OpndSize size,
                      int disp, int base_reg, bool isBasePhysical,
                      int reg, bool isPhysical);
void movez_reg_to_reg(OpndSize size,
                      int reg, bool isPhysical,
                      int reg2, bool isPhysical2);
void moves_mem_to_reg(LowOp* op, OpndSize size,
                      int disp, int base_reg, bool isBasePhysical,
                      int reg, bool isPhysical);
void movez_mem_disp_scale_to_reg(OpndSize size,
                      int base_reg, bool isBasePhysical,
                      int disp, int index_reg, bool isIndexPhysical, int scale,
                      int reg, bool isPhysical);
void moves_mem_disp_scale_to_reg(OpndSize size,
                      int base_reg, bool isBasePhysical,
                      int disp, int index_reg, bool isIndexPhysical, int scale,
                      int reg, bool isPhysical);
void move_reg_to_reg(OpndSize size,
                      int reg, bool isPhysical,
                      int reg2, bool isPhysical2);
void move_reg_to_reg_noalloc(OpndSize size,
                      int reg, bool isPhysical,
                      int reg2, bool isPhysical2);
void move_mem_scale_to_reg(OpndSize size,
                            int base_reg, bool isBasePhysical, int index_reg, bool isIndexPhysical, int scale,
                            int reg, bool isPhysical);
void move_mem_disp_scale_to_reg(OpndSize size,
                int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale,
                int reg, bool isPhysical);
void move_reg_to_mem_scale(OpndSize size,
                            int reg, bool isPhysical,
                            int base_reg, bool isBasePhysical, int index_reg, bool isIndexPhysical, int scale);
void move_reg_to_mem_disp_scale(OpndSize size,
                            int reg, bool isPhysical,
                            int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale);
void move_imm_to_mem(OpndSize size, int imm,
                      int disp, int base_reg, bool isBasePhysical);
void set_VR_to_imm(u2 vA, OpndSize size, int imm);
void set_VR_to_imm_noalloc(u2 vA, OpndSize size, int imm);
void set_VR_to_imm_noupdateref(LowOp* op, u2 vA, OpndSize size, int imm);
void move_imm_to_reg(OpndSize size, int imm, int reg, bool isPhysical);
void move_imm_to_reg_noalloc(OpndSize size, int imm, int reg, bool isPhysical);

//LR[reg] = VR[vB]
//or
//PR[reg] = VR[vB]
void get_virtual_reg(u2 vB, OpndSize size, int reg, bool isPhysical);
void get_virtual_reg_noalloc(u2 vB, OpndSize size, int reg, bool isPhysical);
//VR[v] = LR[reg]
//or
//VR[v] = PR[reg]
void set_virtual_reg(u2 vA, OpndSize size, int reg, bool isPhysical);
void set_virtual_reg_noalloc(u2 vA, OpndSize size, int reg, bool isPhysical);
void get_VR_ss(int vB, int reg, bool isPhysical);
void set_VR_ss(int vA, int reg, bool isPhysical);
void get_VR_sd(int vB, int reg, bool isPhysical);
void set_VR_sd(int vA, int reg, bool isPhysical);

int spill_reg(int reg, bool isPhysical);
int unspill_reg(int reg, bool isPhysical);

void move_reg_to_mem_noalloc(OpndSize size,
                      int reg, bool isPhysical,
                      int disp, int base_reg, bool isBasePhysical,
                      MemoryAccessType mType, int mIndex);
LowOpRegMem* move_mem_to_reg_noalloc(OpndSize size,
                      int disp, int base_reg, bool isBasePhysical,
                      MemoryAccessType mType, int mIndex,
                      int reg, bool isPhysical);

//////////////////////////////////////////////////////////////
int insertLabel(const char* label, bool checkDup);
int export_pc();
int simpleNullCheck(int reg, bool isPhysical, int vr);
int nullCheck(int reg, bool isPhysical, int exceptionNum, int vr);
int handlePotentialException(
                             ConditionCode code_excep, ConditionCode code_okay,
                             int exceptionNum, const char* errName);
int get_currentpc(int reg, bool isPhysical);
int get_self_pointer(int reg, bool isPhysical);
int get_res_strings(int reg, bool isPhysical);
int get_res_classes(int reg, bool isPhysical);
int get_res_fields(int reg, bool isPhysical);
int get_res_methods(int reg, bool isPhysical);
int get_glue_method_class(int reg, bool isPhysical);
int get_glue_method(int reg, bool isPhysical);
int set_glue_method(int reg, bool isPhysical);
int get_glue_dvmdex(int reg, bool isPhysical);
int set_glue_dvmdex(int reg, bool isPhysical);
int get_suspendCount(int reg, bool isPhysical);
int get_return_value(OpndSize size, int reg, bool isPhysical);
int set_return_value(OpndSize size, int reg, bool isPhysical);
int clear_exception();
int get_exception(int reg, bool isPhysical);
int set_exception(int reg, bool isPhysical);
int save_pc_fp_to_glue();
int savearea_from_fp(int reg, bool isPhysical);

int call_moddi3();
int call_divdi3();
int call_fmod();
int call_fmodf();
int call_dvmFindCatchBlock();
int call_dvmThrowVerificationError();
int call_dvmAllocObject();
int call_dvmAllocArrayByClass();
int call_dvmResolveMethod();
int call_dvmResolveClass();
int call_dvmInstanceofNonTrivial();
int call_dvmThrow();
int call_dvmThrowWithMessage();
int call_dvmCheckSuspendPending();
int call_dvmLockObject();
int call_dvmUnlockObject();
int call_dvmInitClass();
int call_dvmAllocPrimitiveArray();
int call_dvmInterpHandleFillArrayData();
int call_dvmNcgHandlePackedSwitch();
int call_dvmNcgHandleSparseSwitch();
int call_dvmJitHandlePackedSwitch();
int call_dvmJitHandleSparseSwitch();
int call_dvmJitToInterpTraceSelectNoChain();
int call_dvmJitToPatchPredictedChain();
int call_dvmJitToInterpNormal();
int call_dvmJitToInterpTraceSelect();
int call_dvmQuasiAtomicSwap64();
int call_dvmQuasiAtomicRead64();
int call_dvmCanPutArrayElement();
int call_dvmFindInterfaceMethodInCache();
int call_dvmHandleStackOverflow();
int call_dvmResolveString();
int call_dvmResolveInstField();
int call_dvmResolveStaticField();

//labels and branches
//shared branch to resolve class: 2 specialized versions
//OPTION 1: call & ret
//OPTION 2: store jump back label in a fixed register or memory
//jump to .class_resolve, then jump back
//OPTION 3: share translator code
/* global variables: ncg_rPC */
int resolve_class(
                  int startLR/*logical register index*/, bool isPhysical, int tmp/*const pool index*/,
                  int thirdArg);
/* EXPORT_PC; movl exceptionPtr, -8(%esp); movl descriptor, -4(%esp); lea; call; lea; jmp */
int throw_exception_message(int exceptionPtr, int obj_reg, bool isPhysical,
                            int startLR/*logical register index*/, bool startPhysical);
/* EXPORT_PC; movl exceptionPtr, -8(%esp); movl imm, -4(%esp); lea; call; lea; jmp */
int throw_exception(int exceptionPtr, int imm,
                    int startLR/*logical register index*/, bool startPhysical);

void freeShortMap();
int insertDataWorklist(s4 relativePC, char* codePtr1);
#ifdef ENABLE_TRACING
int insertMapWorklist(s4 BCOffset, s4 NCGOffset, int isStartOfPC);
#endif
int performNCGWorklist();
int performDataWorklist();
void performLabelWorklist();
void performMethodLabelWorklist();
void freeLabelMap();
void performSharedWorklist();
void performChainingWorklist();
void freeNCGWorklist();
void freeDataWorklist();
void freeLabelWorklist();
void freeChainingWorklist();

int common_invokeArgsDone(ArgsDoneType form, bool isJitFull);
int common_backwardBranch();
int common_exceptionThrown();
int common_errNullObject();
int common_errArrayIndex();
int common_errArrayStore();
int common_errNegArraySize();
int common_errNoSuchMethod();
int common_errDivideByZero();
int common_periodicChecks_entry();
int common_periodicChecks4();
int common_gotoBail();
int common_gotoBail_0();
int common_StringIndexOutOfBounds();
void goto_invokeArgsDone();

//lower a bytecode
int lowerByteCode(const Method* method);

int op_nop();
int op_move();
int op_move_from16();
int op_move_16();
int op_move_wide();
int op_move_wide_from16();
int op_move_wide_16();
int op_move_result();
int op_move_result_wide();
int op_move_exception();

int op_return_void();
int op_return();
int op_return_wide();
int op_const_4();
int op_const_16();
int op_const();
int op_const_high16();
int op_const_wide_16();
int op_const_wide_32();
int op_const_wide();
int op_const_wide_high16();
int op_const_string();
int op_const_string_jumbo();
int op_const_class();
int op_monitor_enter();
int op_monitor_exit();
int op_check_cast();
int op_instance_of();

int op_array_length();
int op_new_instance();
int op_new_array();
int op_filled_new_array();
int op_filled_new_array_range();
int op_fill_array_data();
int op_throw();
int op_throw_verification_error();
int op_goto();
int op_goto_16();
int op_goto_32();
int op_packed_switch();
int op_sparse_switch();
int op_if_ge();
int op_aget();
int op_aget_wide();
int op_aget_object();
int op_aget_boolean();
int op_aget_byte();
int op_aget_char();
int op_aget_short();
int op_aput();
int op_aput_wide();
int op_aput_object();
int op_aput_boolean();
int op_aput_byte();
int op_aput_char();
int op_aput_short();
int op_iget();
int op_iget_wide(bool isVolatile);
int op_iget_object();
int op_iget_boolean();
int op_iget_byte();
int op_iget_char();
int op_iget_short();
int op_iput();
int op_iput_wide(bool isVolatile);
int op_iput_object();
int op_iput_boolean();
int op_iput_byte();
int op_iput_char();
int op_iput_short();
int op_sget();
int op_sget_wide(bool isVolatile);
int op_sget_object();
int op_sget_boolean();
int op_sget_byte();
int op_sget_char();
int op_sget_short();
int op_sput(bool isObj);
int op_sput_wide(bool isVolatile);
int op_sput_object();
int op_sput_boolean();
int op_sput_byte();
int op_sput_char();
int op_sput_short();
int op_invoke_virtual();
int op_invoke_super();
int op_invoke_direct();
int op_invoke_static();
int op_invoke_interface();
int op_invoke_virtual_range();
int op_invoke_super_range();
int op_invoke_direct_range();
int op_invoke_static_range();
int op_invoke_interface_range();
int op_int_to_long();
int op_add_long_2addr();
int op_add_int_lit8();
int op_cmpl_float();
int op_cmpg_float();
int op_cmpl_double();
int op_cmpg_double();
int op_cmp_long();
int op_if_eq();
int op_if_ne();
int op_if_lt();
int op_if_gt();
int op_if_le();
int op_if_eqz();
int op_if_nez();
int op_if_ltz();
int op_if_gez();
int op_if_gtz();
int op_if_lez();
int op_neg_int();
int op_not_int();
int op_neg_long();
int op_not_long();
int op_neg_float();
int op_neg_double();
int op_int_to_float();
int op_int_to_double();
int op_long_to_int();
int op_long_to_float();
int op_long_to_double();
int op_float_to_int();
int op_float_to_long();
int op_float_to_double();
int op_double_to_int();
int op_double_to_long();
int op_double_to_float();
int op_int_to_byte();
int op_int_to_char();
int op_int_to_short();
int op_add_int();
int op_sub_int();
int op_mul_int();
int op_div_int();
int op_rem_int();
int op_and_int();
int op_or_int();
int op_xor_int();
int op_shl_int();
int op_shr_int();
int op_ushr_int();
int op_add_long();
int op_sub_long();
int op_mul_long();
int op_div_long();
int op_rem_long();
int op_and_long();
int op_or_long();
int op_xor_long();
int op_shl_long();
int op_shr_long();
int op_ushr_long();
int op_add_float();
int op_sub_float();
int op_mul_float();
int op_div_float();
int op_rem_float();
int op_add_double();
int op_sub_double();
int op_mul_double();
int op_div_double();
int op_rem_double();
int op_add_int_2addr();
int op_sub_int_2addr();
int op_mul_int_2addr();
int op_div_int_2addr();
int op_rem_int_2addr();
int op_and_int_2addr();
int op_or_int_2addr();
int op_xor_int_2addr();
int op_shl_int_2addr();
int op_shr_int_2addr();
int op_ushr_int_2addr();
int op_sub_long_2addr();
int op_mul_long_2addr();
int op_div_long_2addr();
int op_rem_long_2addr();
int op_and_long_2addr();
int op_or_long_2addr();
int op_xor_long_2addr();
int op_shl_long_2addr();
int op_shr_long_2addr();
int op_ushr_long_2addr();
int op_add_float_2addr();
int op_sub_float_2addr();
int op_mul_float_2addr();
int op_div_float_2addr();
int op_rem_float_2addr();
int op_add_double_2addr();
int op_sub_double_2addr();
int op_mul_double_2addr();
int op_div_double_2addr();
int op_rem_double_2addr();
int op_add_int_lit16();
int op_rsub_int();
int op_mul_int_lit16();
int op_div_int_lit16();
int op_rem_int_lit16();
int op_and_int_lit16();
int op_or_int_lit16();
int op_xor_int_lit16();
int op_rsub_int_lit8();
int op_mul_int_lit8();
int op_div_int_lit8();
int op_rem_int_lit8();
int op_and_int_lit8();
int op_or_int_lit8();
int op_xor_int_lit8();
int op_shl_int_lit8();
int op_shr_int_lit8();
int op_ushr_int_lit8();
int op_execute_inline(bool isRange);
int op_invoke_object_init_range();
int op_iget_quick();
int op_iget_wide_quick();
int op_iget_object_quick();
int op_iput_quick();
int op_iput_wide_quick();
int op_iput_object_quick();
int op_invoke_virtual_quick();
int op_invoke_virtual_quick_range();
int op_invoke_super_quick();
int op_invoke_super_quick_range();

///////////////////////////////////////////////
void set_reg_opnd(LowOpndReg* op_reg, int reg, bool isPhysical, LowOpndRegType type);
void set_mem_opnd(LowOpndMem* mem, int disp, int base, bool isPhysical);
void set_mem_opnd_scale(LowOpndMem* mem, int base, bool isPhysical, int disp, int index, bool indexPhysical, int scale);
LowOpImm* dump_imm(Mnemonic m, OpndSize size,
               int imm);
LowOpNCG* dump_ncg(Mnemonic m, OpndSize size, int imm);
LowOpImm* dump_imm_with_codeaddr(Mnemonic m, OpndSize size,
               int imm, char* codePtr);
LowOpImm* dump_special(AtomOpCode cc, int imm);
LowOpMem* dump_mem(Mnemonic m, AtomOpCode m2, OpndSize size,
               int disp, int base_reg, bool isBasePhysical);
LowOpReg* dump_reg(Mnemonic m, AtomOpCode m2, OpndSize size,
               int reg, bool isPhysical, LowOpndRegType type);
LowOpReg* dump_reg_noalloc(Mnemonic m, OpndSize size,
               int reg, bool isPhysical, LowOpndRegType type);
LowOpMemImm* dump_imm_mem_noalloc(Mnemonic m, OpndSize size,
                           int imm,
                           int disp, int base_reg, bool isBasePhysical,
                           MemoryAccessType mType, int mIndex);
LowOpRegReg* dump_reg_reg(Mnemonic m, AtomOpCode m2, OpndSize size,
                   int reg, bool isPhysical,
                   int reg2, bool isPhysical2, LowOpndRegType type);
LowOpRegReg* dump_movez_reg_reg(Mnemonic m, OpndSize size,
                        int reg, bool isPhysical,
                        int reg2, bool isPhysical2);
LowOpRegMem* dump_mem_reg(Mnemonic m, AtomOpCode m2, OpndSize size,
                   int disp, int base_reg, bool isBasePhysical,
                   MemoryAccessType mType, int mIndex,
                   int reg, bool isPhysical, LowOpndRegType type);
LowOpRegMem* dump_mem_reg_noalloc(Mnemonic m, OpndSize size,
                           int disp, int base_reg, bool isBasePhysical,
                           MemoryAccessType mType, int mIndex,
                           int reg, bool isPhysical, LowOpndRegType type);
LowOpRegMem* dump_mem_scale_reg(Mnemonic m, OpndSize size,
                         int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale,
                         int reg, bool isPhysical, LowOpndRegType type);
LowOpMemReg* dump_reg_mem_scale(Mnemonic m, OpndSize size,
                         int reg, bool isPhysical,
                         int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale,
                         LowOpndRegType type);
LowOpMemReg* dump_reg_mem(Mnemonic m, AtomOpCode m2, OpndSize size,
                   int reg, bool isPhysical,
                   int disp, int base_reg, bool isBasePhysical,
                   MemoryAccessType mType, int mIndex, LowOpndRegType type);
LowOpMemReg* dump_reg_mem_noalloc(Mnemonic m, OpndSize size,
                           int reg, bool isPhysical,
                           int disp, int base_reg, bool isBasePhysical,
                           MemoryAccessType mType, int mIndex, LowOpndRegType type);
LowOpRegImm* dump_imm_reg(Mnemonic m, AtomOpCode m2, OpndSize size,
                   int imm, int reg, bool isPhysical, LowOpndRegType type, bool chaining);
LowOpMemImm* dump_imm_mem(Mnemonic m, AtomOpCode m2, OpndSize size,
                   int imm,
                   int disp, int base_reg, bool isBasePhysical,
                   MemoryAccessType mType, int mIndex, bool chaining);
LowOpMemReg* dump_fp_mem(Mnemonic m, OpndSize size, int reg,
                  int disp, int base_reg, bool isBasePhysical,
                  MemoryAccessType mType, int mIndex);
LowOpRegMem* dump_mem_fp(Mnemonic m, OpndSize size,
                  int disp, int base_reg, bool isBasePhysical,
                  MemoryAccessType mType, int mIndex,
                  int reg);
LowOpLabel* dump_label(Mnemonic m, OpndSize size, int imm,
               const char* label, bool isLocal);

unsigned getJmpCallInstSize(OpndSize size, JmpCall_type type);
bool lowerByteCodeJit(const Method* method, const u2* codePtr, MIR* mir);
void startOfBasicBlock(struct BasicBlock* bb);
extern LowOpBlockLabel* traceLabelList;
extern struct BasicBlock* traceCurrentBB;
extern struct MIR* traceCurrentMIR;
void startOfTrace(const Method* method, LowOpBlockLabel* labelList, int, CompilationUnit*);
void endOfTrace(bool freeOnly);
LowOp* jumpToBasicBlock(char* instAddr, int targetId);
LowOp* condJumpToBasicBlock(char* instAddr, ConditionCode cc, int targetId);
bool jumpToException(const char* target);
int codeGenBasicBlockJit(const Method* method, BasicBlock* bb);
void endOfBasicBlock(struct BasicBlock* bb);
void handleExtendedMIR(CompilationUnit *cUnit, MIR *mir);
int insertChainingWorklist(int bbId, char * codeStart);
void startOfTraceO1(const Method* method, LowOpBlockLabel* labelList, int exceptionBlockId, CompilationUnit *cUnit);
void endOfTraceO1();
int isPowerOfTwo(int imm);
void move_chain_to_mem(OpndSize size, int imm,
                        int disp, int base_reg, bool isBasePhysical);
void move_chain_to_reg(OpndSize size, int imm, int reg, bool isPhysical);

void dumpImmToMem(int vrNum, OpndSize size, int value);
bool isInMemory(int regNum, OpndSize size);
int touchEbx();
int boundCheck(int vr_array, int reg_array, bool isPhysical_array,
               int vr_index, int reg_index, bool isPhysical_index,
               int exceptionNum);
int getRelativeOffset(const char* target, bool isShortTerm, JmpCall_type type, bool* unknown,
                      OpndSize* immSize);
int getRelativeNCG(s4 tmp, JmpCall_type type, bool* unknown, OpndSize* size);
void freeAtomMem();
OpndSize estOpndSizeFromImm(int target);

void preprocessingBB(BasicBlock* bb);
void preprocessingTrace();
void dump_nop(int size);
#endif
