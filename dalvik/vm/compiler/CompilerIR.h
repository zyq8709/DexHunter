/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef DALVIK_VM_COMPILER_IR_H_
#define DALVIK_VM_COMPILER_IR_H_

#include "codegen/Optimizer.h"
#ifdef ARCH_IA32
#include "CompilerUtility.h"
#endif

typedef enum RegisterClass {
    kCoreReg,
    kFPReg,
    kAnyReg,
} RegisterClass;

typedef enum RegLocationType {
    kLocDalvikFrame = 0,
    kLocPhysReg,
    kLocRetval,          // Return region in interpState
    kLocSpill,
} RegLocationType;

typedef struct RegLocation {
    RegLocationType location:2;
    unsigned wide:1;
    unsigned fp:1;      // Hint for float/double
    u1 lowReg:6;        // First physical register
    u1 highReg:6;       // 2nd physical register (if wide)
    s2 sRegLow;         // SSA name for low Dalvik word
} RegLocation;

#define INVALID_SREG (-1)
#define INVALID_REG (0x3F)

typedef enum BBType {
    /* For coding convenience reasons chaining cell types should appear first */
    kChainingCellNormal = 0,
    kChainingCellHot,
    kChainingCellInvokeSingleton,
    kChainingCellInvokePredicted,
    kChainingCellBackwardBranch,
    kChainingCellGap,
    /* Don't insert new fields between Gap and Last */
    kChainingCellLast = kChainingCellGap + 1,
    kEntryBlock,
    kDalvikByteCode,
    kExitBlock,
    kPCReconstruction,
    kExceptionHandling,
    kCatchEntry,
} BBType;

typedef enum JitMode {
    kJitTrace = 0, // Acyclic - all instructions come from the trace descriptor
    kJitLoop,      // Cycle - trace descriptor is used as a hint
    kJitMethod,    // Whole method
} JitMode;

typedef struct ChainCellCounts {
    union {
        u1 count[kChainingCellLast]; /* include one more space for the gap # */
        u4 dummyForAlignment;
    } u;
} ChainCellCounts;

typedef struct LIR {
    int offset;
    struct LIR *next;
    struct LIR *prev;
    struct LIR *target;
} LIR;

enum ExtendedMIROpcode {
    kMirOpFirst = kNumPackedOpcodes,
    kMirOpPhi = kMirOpFirst,
    kMirOpNullNRangeUpCheck,
    kMirOpNullNRangeDownCheck,
    kMirOpLowerBound,
    kMirOpPunt,
    kMirOpCheckInlinePrediction,        // Gen checks for predicted inlining
    kMirOpLast,
};

struct SSARepresentation;

typedef enum {
    kMIRIgnoreNullCheck = 0,
    kMIRNullCheckOnly,
    kMIRIgnoreRangeCheck,
    kMIRRangeCheckOnly,
    kMIRInlined,                        // Invoke is inlined (ie dead)
    kMIRInlinedPred,                    // Invoke is inlined via prediction
    kMIRCallee,                         // Instruction is inlined from callee
    kMIRInvokeMethodJIT,                // Callee is JIT'ed as a whole method
} MIROptimizationFlagPositons;

#define MIR_IGNORE_NULL_CHECK           (1 << kMIRIgnoreNullCheck)
#define MIR_NULL_CHECK_ONLY             (1 << kMIRNullCheckOnly)
#define MIR_IGNORE_RANGE_CHECK          (1 << kMIRIgnoreRangeCheck)
#define MIR_RANGE_CHECK_ONLY            (1 << kMIRRangeCheckOnly)
#define MIR_INLINED                     (1 << kMIRInlined)
#define MIR_INLINED_PRED                (1 << kMIRInlinedPred)
#define MIR_CALLEE                      (1 << kMIRCallee)
#define MIR_INVOKE_METHOD_JIT           (1 << kMIRInvokeMethodJIT)

typedef struct CallsiteInfo {
    const char *classDescriptor;
    Object *classLoader;
    const Method *method;
    LIR *misPredBranchOver;
} CallsiteInfo;

typedef struct MIR {
    DecodedInstruction dalvikInsn;
    unsigned int width;
    unsigned int offset;
    struct MIR *prev;
    struct MIR *next;
    struct SSARepresentation *ssaRep;
    int OptimizationFlags;
    int seqNum;
    union {
        // Used by the inlined insn from the callee to find the mother method
        const Method *calleeMethod;
        // Used by the inlined invoke to find the class and method pointers
        CallsiteInfo *callsiteInfo;
    } meta;
} MIR;

struct BasicBlockDataFlow;

/* For successorBlockList */
typedef enum BlockListType {
    kNotUsed = 0,
    kCatch,
    kPackedSwitch,
    kSparseSwitch,
} BlockListType;

typedef struct BasicBlock {
    int id;
    bool visited;
    bool hidden;
    unsigned int startOffset;
    const Method *containingMethod;     // For blocks from the callee
    BBType blockType;
    bool needFallThroughBranch;         // For blocks ended due to length limit
    bool isFallThroughFromInvoke;       // True means the block needs alignment
    MIR *firstMIRInsn;
    MIR *lastMIRInsn;
    struct BasicBlock *fallThrough;
    struct BasicBlock *taken;
    struct BasicBlock *iDom;            // Immediate dominator
    struct BasicBlockDataFlow *dataFlowInfo;
    BitVector *predecessors;
    BitVector *dominators;
    BitVector *iDominated;              // Set nodes being immediately dominated
    BitVector *domFrontier;             // Dominance frontier
    struct {                            // For one-to-many successors like
        BlockListType blockListType;    // switch and exception handling
        GrowableList blocks;
    } successorBlockList;
} BasicBlock;

/*
 * The "blocks" field in "successorBlockList" points to an array of
 * elements with the type "SuccessorBlockInfo".
 * For catch blocks, key is type index for the exception.
 * For swtich blocks, key is the case value.
 */
typedef struct SuccessorBlockInfo {
    BasicBlock *block;
    int key;
} SuccessorBlockInfo;

struct LoopAnalysis;
struct RegisterPool;

typedef enum AssemblerStatus {
    kSuccess,
    kRetryAll,
    kRetryHalve
} AssemblerStatus;

typedef struct CompilationUnit {
    int numInsts;
    int numBlocks;
    GrowableList blockList;
    const Method *method;
#ifdef ARCH_IA32
    int exceptionBlockId;               // the block corresponding to exception handling
#endif
    const JitTraceDescription *traceDesc;
    LIR *firstLIRInsn;
    LIR *lastLIRInsn;
    LIR *literalList;                   // Constants
    LIR *classPointerList;              // Relocatable
    int numClassPointers;
    LIR *chainCellOffsetLIR;
    GrowableList pcReconstructionList;
    int headerSize;                     // bytes before the first code ptr
    int dataOffset;                     // starting offset of literal pool
    int totalSize;                      // header + code size
    AssemblerStatus assemblerStatus;    // Success or fix and retry
    int assemblerRetries;               // How many times tried to fix assembly
    unsigned char *codeBuffer;
    void *baseAddr;
    bool printMe;
    bool allSingleStep;
    bool hasClassLiterals;              // Contains class ptrs used as literals
    bool hasLoop;                       // Contains a loop
    bool hasInvoke;                     // Contains an invoke instruction
    bool heapMemOp;                     // Mark mem ops for self verification
    bool usesLinkRegister;              // For self-verification only
    int profileCodeSize;                // Size of the profile prefix in bytes
    int numChainingCells[kChainingCellGap];
    LIR *firstChainingLIR[kChainingCellGap];
    LIR *chainingCellBottom;
    struct RegisterPool *regPool;
    int optRound;                       // round number to tell an LIR's age
    jmp_buf *bailPtr;
    JitInstructionSetType instructionSet;
    /* Number of total regs used in the whole cUnit after SSA transformation */
    int numSSARegs;
    /* Map SSA reg i to the Dalvik[15..0]/Sub[31..16] pair. */
    GrowableList *ssaToDalvikMap;

    /* The following are new data structures to support SSA representations */
    /* Map original Dalvik reg i to the SSA[15..0]/Sub[31..16] pair */
    int *dalvikToSSAMap;                // length == method->registersSize
    BitVector *isConstantV;             // length == numSSAReg
    int *constantValues;                // length == numSSAReg

    /* Data structure for loop analysis and optimizations */
    struct LoopAnalysis *loopAnalysis;

    /* Map SSA names to location */
    RegLocation *regLocation;
    int sequenceNumber;

    /*
     * Set to the Dalvik PC of the switch instruction if it has more than
     * MAX_CHAINED_SWITCH_CASES cases.
     */
    const u2 *switchOverflowPad;

    JitMode jitMode;
    int numReachableBlocks;
    int numDalvikRegisters;             // method->registersSize + inlined
    BasicBlock *entryBlock;
    BasicBlock *exitBlock;
    BasicBlock *puntBlock;              // punting to interp for exceptions
    BasicBlock *backChainBlock;         // for loop-trace
    BasicBlock *curBlock;
    BasicBlock *nextCodegenBlock;       // for extended trace codegen
    GrowableList dfsOrder;
    GrowableList domPostOrderTraversal;
    BitVector *tryBlockAddr;
    BitVector **defBlockMatrix;         // numDalvikRegister x numBlocks
    BitVector *tempBlockV;
    BitVector *tempDalvikRegisterV;
    BitVector *tempSSARegisterV;        // numSSARegs
    bool printSSANames;
    void *blockLabelList;
    bool quitLoopMode;                  // cold path/complex bytecode
} CompilationUnit;

#if defined(WITH_SELF_VERIFICATION)
#define HEAP_ACCESS_SHADOW(_state) cUnit->heapMemOp = _state
#else
#define HEAP_ACCESS_SHADOW(_state)
#endif

BasicBlock *dvmCompilerNewBB(BBType blockType, int blockId);

void dvmCompilerAppendMIR(BasicBlock *bb, MIR *mir);

void dvmCompilerPrependMIR(BasicBlock *bb, MIR *mir);

void dvmCompilerInsertMIRAfter(BasicBlock *bb, MIR *currentMIR, MIR *newMIR);

void dvmCompilerAppendLIR(CompilationUnit *cUnit, LIR *lir);

void dvmCompilerInsertLIRBefore(LIR *currentLIR, LIR *newLIR);

void dvmCompilerInsertLIRAfter(LIR *currentLIR, LIR *newLIR);

void dvmCompilerAbort(CompilationUnit *cUnit);

/* Debug Utilities */
void dvmCompilerDumpCompilationUnit(CompilationUnit *cUnit);

#endif  // DALVIK_VM_COMPILER_IR_H_
