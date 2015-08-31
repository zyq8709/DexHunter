/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_COMPILER_DEX_MIR_GRAPH_H_
#define ART_COMPILER_DEX_MIR_GRAPH_H_

#include "dex_file.h"
#include "dex_instruction.h"
#include "compiler_ir.h"
#include "arena_bit_vector.h"
#include "growable_array.h"

namespace art {

enum InstructionAnalysisAttributePos {
  kUninterestingOp = 0,
  kArithmeticOp,
  kFPOp,
  kSingleOp,
  kDoubleOp,
  kIntOp,
  kLongOp,
  kBranchOp,
  kInvokeOp,
  kArrayOp,
  kHeavyweightOp,
  kSimpleConstOp,
  kMoveOp,
  kSwitch
};

#define AN_NONE (1 << kUninterestingOp)
#define AN_MATH (1 << kArithmeticOp)
#define AN_FP (1 << kFPOp)
#define AN_LONG (1 << kLongOp)
#define AN_INT (1 << kIntOp)
#define AN_SINGLE (1 << kSingleOp)
#define AN_DOUBLE (1 << kDoubleOp)
#define AN_FLOATMATH (1 << kFPOp)
#define AN_BRANCH (1 << kBranchOp)
#define AN_INVOKE (1 << kInvokeOp)
#define AN_ARRAYOP (1 << kArrayOp)
#define AN_HEAVYWEIGHT (1 << kHeavyweightOp)
#define AN_SIMPLECONST (1 << kSimpleConstOp)
#define AN_MOVE (1 << kMoveOp)
#define AN_SWITCH (1 << kSwitch)
#define AN_COMPUTATIONAL (AN_MATH | AN_ARRAYOP | AN_MOVE | AN_SIMPLECONST)

enum DataFlowAttributePos {
  kUA = 0,
  kUB,
  kUC,
  kAWide,
  kBWide,
  kCWide,
  kDA,
  kIsMove,
  kSetsConst,
  kFormat35c,
  kFormat3rc,
  kNullCheckSrc0,        // Null check of uses[0].
  kNullCheckSrc1,        // Null check of uses[1].
  kNullCheckSrc2,        // Null check of uses[2].
  kNullCheckOut0,        // Null check out outgoing arg0.
  kDstNonNull,           // May assume dst is non-null.
  kRetNonNull,           // May assume retval is non-null.
  kNullTransferSrc0,     // Object copy src[0] -> dst.
  kNullTransferSrcN,     // Phi null check state transfer.
  kRangeCheckSrc1,       // Range check of uses[1].
  kRangeCheckSrc2,       // Range check of uses[2].
  kRangeCheckSrc3,       // Range check of uses[3].
  kFPA,
  kFPB,
  kFPC,
  kCoreA,
  kCoreB,
  kCoreC,
  kRefA,
  kRefB,
  kRefC,
  kUsesMethodStar,       // Implicit use of Method*.
};

#define DF_NOP                  0
#define DF_UA                   (1 << kUA)
#define DF_UB                   (1 << kUB)
#define DF_UC                   (1 << kUC)
#define DF_A_WIDE               (1 << kAWide)
#define DF_B_WIDE               (1 << kBWide)
#define DF_C_WIDE               (1 << kCWide)
#define DF_DA                   (1 << kDA)
#define DF_IS_MOVE              (1 << kIsMove)
#define DF_SETS_CONST           (1 << kSetsConst)
#define DF_FORMAT_35C           (1 << kFormat35c)
#define DF_FORMAT_3RC           (1 << kFormat3rc)
#define DF_NULL_CHK_0           (1 << kNullCheckSrc0)
#define DF_NULL_CHK_1           (1 << kNullCheckSrc1)
#define DF_NULL_CHK_2           (1 << kNullCheckSrc2)
#define DF_NULL_CHK_OUT0        (1 << kNullCheckOut0)
#define DF_NON_NULL_DST         (1 << kDstNonNull)
#define DF_NON_NULL_RET         (1 << kRetNonNull)
#define DF_NULL_TRANSFER_0      (1 << kNullTransferSrc0)
#define DF_NULL_TRANSFER_N      (1 << kNullTransferSrcN)
#define DF_RANGE_CHK_1          (1 << kRangeCheckSrc1)
#define DF_RANGE_CHK_2          (1 << kRangeCheckSrc2)
#define DF_RANGE_CHK_3          (1 << kRangeCheckSrc3)
#define DF_FP_A                 (1 << kFPA)
#define DF_FP_B                 (1 << kFPB)
#define DF_FP_C                 (1 << kFPC)
#define DF_CORE_A               (1 << kCoreA)
#define DF_CORE_B               (1 << kCoreB)
#define DF_CORE_C               (1 << kCoreC)
#define DF_REF_A                (1 << kRefA)
#define DF_REF_B                (1 << kRefB)
#define DF_REF_C                (1 << kRefC)
#define DF_UMS                  (1 << kUsesMethodStar)

#define DF_HAS_USES             (DF_UA | DF_UB | DF_UC)

#define DF_HAS_DEFS             (DF_DA)

#define DF_HAS_NULL_CHKS        (DF_NULL_CHK_0 | \
                                 DF_NULL_CHK_1 | \
                                 DF_NULL_CHK_2 | \
                                 DF_NULL_CHK_OUT0)

#define DF_HAS_RANGE_CHKS       (DF_RANGE_CHK_1 | \
                                 DF_RANGE_CHK_2 | \
                                 DF_RANGE_CHK_3)

#define DF_HAS_NR_CHKS          (DF_HAS_NULL_CHKS | \
                                 DF_HAS_RANGE_CHKS)

#define DF_A_IS_REG             (DF_UA | DF_DA)
#define DF_B_IS_REG             (DF_UB)
#define DF_C_IS_REG             (DF_UC)
#define DF_IS_GETTER_OR_SETTER  (DF_IS_GETTER | DF_IS_SETTER)
#define DF_USES_FP              (DF_FP_A | DF_FP_B | DF_FP_C)

enum OatMethodAttributes {
  kIsLeaf,            // Method is leaf.
  kHasLoop,           // Method contains simple loop.
};

#define METHOD_IS_LEAF          (1 << kIsLeaf)
#define METHOD_HAS_LOOP         (1 << kHasLoop)

// Minimum field size to contain Dalvik v_reg number.
#define VREG_NUM_WIDTH 16

#define INVALID_SREG (-1)
#define INVALID_VREG (0xFFFFU)
#define INVALID_REG (0xFF)
#define INVALID_OFFSET (0xDEADF00FU)

/* SSA encodings for special registers */
#define SSA_METHOD_BASEREG (-2)
/* First compiler temp basereg, grows smaller */
#define SSA_CTEMP_BASEREG (SSA_METHOD_BASEREG - 1)

#define MIR_IGNORE_NULL_CHECK           (1 << kMIRIgnoreNullCheck)
#define MIR_NULL_CHECK_ONLY             (1 << kMIRNullCheckOnly)
#define MIR_IGNORE_RANGE_CHECK          (1 << kMIRIgnoreRangeCheck)
#define MIR_RANGE_CHECK_ONLY            (1 << kMIRRangeCheckOnly)
#define MIR_INLINED                     (1 << kMIRInlined)
#define MIR_INLINED_PRED                (1 << kMIRInlinedPred)
#define MIR_CALLEE                      (1 << kMIRCallee)
#define MIR_IGNORE_SUSPEND_CHECK        (1 << kMIRIgnoreSuspendCheck)
#define MIR_DUP                         (1 << kMIRDup)

#define BLOCK_NAME_LEN 80

/*
 * In general, vreg/sreg describe Dalvik registers that originated with dx.  However,
 * it is useful to have compiler-generated temporary registers and have them treated
 * in the same manner as dx-generated virtual registers.  This struct records the SSA
 * name of compiler-introduced temporaries.
 */
struct CompilerTemp {
  int s_reg;
};

// When debug option enabled, records effectiveness of null and range check elimination.
struct Checkstats {
  int null_checks;
  int null_checks_eliminated;
  int range_checks;
  int range_checks_eliminated;
};

// Dataflow attributes of a basic block.
struct BasicBlockDataFlow {
  ArenaBitVector* use_v;
  ArenaBitVector* def_v;
  ArenaBitVector* live_in_v;
  ArenaBitVector* phi_v;
  int* vreg_to_ssa_map;
  ArenaBitVector* ending_null_check_v;
};

/*
 * Normalized use/def for a MIR operation using SSA names rather than vregs.  Note that
 * uses/defs retain the Dalvik convention that long operations operate on a pair of 32-bit
 * vregs.  For example, "ADD_LONG v0, v2, v3" would have 2 defs (v0/v1) and 4 uses (v2/v3, v4/v5).
 * Following SSA renaming, this is the primary struct used by code generators to locate
 * operand and result registers.  This is a somewhat confusing and unhelpful convention that
 * we may want to revisit in the future.
 */
struct SSARepresentation {
  int num_uses;
  int* uses;
  bool* fp_use;
  int num_defs;
  int* defs;
  bool* fp_def;
};

/*
 * The Midlevel Intermediate Representation node, which may be largely considered a
 * wrapper around a Dalvik byte code.
 */
struct MIR {
  DecodedInstruction dalvikInsn;
  uint32_t width;                 // NOTE: only need 16 bits for width.
  unsigned int offset;
  int m_unit_index;               // From which method was this MIR included
  MIR* prev;
  MIR* next;
  SSARepresentation* ssa_rep;
  int optimization_flags;
  union {
    // Establish link between two halves of throwing instructions.
    MIR* throw_insn;
    // Saved opcode for NOP'd MIRs
    Instruction::Code original_opcode;
  } meta;
};

struct SuccessorBlockInfo;

struct BasicBlock {
  int id;
  int dfs_id;
  bool visited;
  bool hidden;
  bool catch_entry;
  bool explicit_throw;
  bool conditional_branch;
  bool terminated_by_return;        // Block ends with a Dalvik return opcode.
  bool dominates_return;            // Is a member of return extended basic block.
  uint16_t start_offset;
  uint16_t nesting_depth;
  BBType block_type;
  MIR* first_mir_insn;
  MIR* last_mir_insn;
  BasicBlock* fall_through;
  BasicBlock* taken;
  BasicBlock* i_dom;                // Immediate dominator.
  BasicBlockDataFlow* data_flow_info;
  GrowableArray<BasicBlock*>* predecessors;
  ArenaBitVector* dominators;
  ArenaBitVector* i_dominated;      // Set nodes being immediately dominated.
  ArenaBitVector* dom_frontier;     // Dominance frontier.
  struct {                          // For one-to-many successors like.
    BlockListType block_list_type;  // switch and exception handling.
    GrowableArray<SuccessorBlockInfo*>* blocks;
  } successor_block_list;
};

/*
 * The "blocks" field in "successor_block_list" points to an array of elements with the type
 * "SuccessorBlockInfo".  For catch blocks, key is type index for the exception.  For swtich
 * blocks, key is the case value.
 */
// TODO: make class with placement new.
struct SuccessorBlockInfo {
  BasicBlock* block;
  int key;
};

/*
 * Whereas a SSA name describes a definition of a Dalvik vreg, the RegLocation describes
 * the type of an SSA name (and, can also be used by code generators to record where the
 * value is located (i.e. - physical register, frame, spill, etc.).  For each SSA name (SReg)
 * there is a RegLocation.
 * FIXME: The orig_sreg field was added as a workaround for llvm bitcode generation.  With
 * the latest restructuring, we should be able to remove it and rely on s_reg_low throughout.
 */
struct RegLocation {
  RegLocationType location:3;
  unsigned wide:1;
  unsigned defined:1;   // Do we know the type?
  unsigned is_const:1;  // Constant, value in mir_graph->constant_values[].
  unsigned fp:1;        // Floating point?
  unsigned core:1;      // Non-floating point?
  unsigned ref:1;       // Something GC cares about.
  unsigned high_word:1;  // High word of pair?
  unsigned home:1;      // Does this represent the home location?
  uint8_t low_reg;      // First physical register.
  uint8_t high_reg;     // 2nd physical register (if wide).
  int32_t s_reg_low;    // SSA name for low Dalvik word.
  int32_t orig_sreg;    // TODO: remove after Bitcode gen complete
                        // and consolodate usage w/ s_reg_low.
};

/*
 * Collection of information describing an invoke, and the destination of
 * the subsequent MOVE_RESULT (if applicable).  Collected as a unit to enable
 * more efficient invoke code generation.
 */
struct CallInfo {
  int num_arg_words;    // Note: word count, not arg count.
  RegLocation* args;    // One for each word of arguments.
  RegLocation result;   // Eventual target of MOVE_RESULT.
  int opt_flags;
  InvokeType type;
  uint32_t dex_idx;
  uint32_t index;       // Method idx for invokes, type idx for FilledNewArray.
  uintptr_t direct_code;
  uintptr_t direct_method;
  RegLocation target;    // Target of following move_result.
  bool skip_this;
  bool is_range;
  int offset;            // Dalvik offset.
};


const RegLocation bad_loc = {kLocDalvikFrame, 0, 0, 0, 0, 0, 0, 0, 0,
                             INVALID_REG, INVALID_REG, INVALID_SREG, INVALID_SREG};

class MIRGraph {
 public:
  MIRGraph(CompilationUnit* cu, ArenaAllocator* arena);
  ~MIRGraph();

  /*
   * Examine the graph to determine whether it's worthwile to spend the time compiling
   * this method.
   */
  bool SkipCompilation(Runtime::CompilerFilter compiler_filter);

  /*
   * Parse dex method and add MIR at current insert point.  Returns id (which is
   * actually the index of the method in the m_units_ array).
   */
  void InlineMethod(const DexFile::CodeItem* code_item, uint32_t access_flags,
                    InvokeType invoke_type, uint16_t class_def_idx,
                    uint32_t method_idx, jobject class_loader, const DexFile& dex_file);

  /* Find existing block */
  BasicBlock* FindBlock(unsigned int code_offset) {
    return FindBlock(code_offset, false, false, NULL);
  }

  const uint16_t* GetCurrentInsns() const {
    return current_code_item_->insns_;
  }

  const uint16_t* GetInsns(int m_unit_index) const {
    return m_units_[m_unit_index]->GetCodeItem()->insns_;
  }

  int GetNumBlocks() const {
    return num_blocks_;
  }

  size_t GetNumDalvikInsns() const {
    return cu_->code_item->insns_size_in_code_units_;
  }

  ArenaBitVector* GetTryBlockAddr() const {
    return try_block_addr_;
  }

  BasicBlock* GetEntryBlock() const {
    return entry_block_;
  }

  BasicBlock* GetExitBlock() const {
    return exit_block_;
  }

  BasicBlock* GetBasicBlock(int block_id) const {
    return block_list_.Get(block_id);
  }

  size_t GetBasicBlockListCount() const {
    return block_list_.Size();
  }

  GrowableArray<BasicBlock*>* GetBlockList() {
    return &block_list_;
  }

  GrowableArray<int>* GetDfsOrder() {
    return dfs_order_;
  }

  GrowableArray<int>* GetDfsPostOrder() {
    return dfs_post_order_;
  }

  GrowableArray<int>* GetDomPostOrder() {
    return dom_post_order_traversal_;
  }

  int GetDefCount() const {
    return def_count_;
  }

  ArenaAllocator* GetArena() {
    return arena_;
  }

  void EnableOpcodeCounting() {
    opcode_count_ = static_cast<int*>(arena_->Alloc(kNumPackedOpcodes * sizeof(int),
                                                    ArenaAllocator::kAllocMisc));
  }

  void ShowOpcodeStats();

  DexCompilationUnit* GetCurrentDexCompilationUnit() const {
    return m_units_[current_method_];
  }

  void DumpCFG(const char* dir_prefix, bool all_blocks);

  void BuildRegLocations();

  void DumpRegLocTable(RegLocation* table, int count);

  void BasicBlockOptimization();

  bool IsConst(int32_t s_reg) const {
    return is_constant_v_->IsBitSet(s_reg);
  }

  bool IsConst(RegLocation loc) const {
    return (IsConst(loc.orig_sreg));
  }

  int32_t ConstantValue(RegLocation loc) const {
    DCHECK(IsConst(loc));
    return constant_values_[loc.orig_sreg];
  }

  int32_t ConstantValue(int32_t s_reg) const {
    DCHECK(IsConst(s_reg));
    return constant_values_[s_reg];
  }

  int64_t ConstantValueWide(RegLocation loc) const {
    DCHECK(IsConst(loc));
    return (static_cast<int64_t>(constant_values_[loc.orig_sreg + 1]) << 32) |
        Low32Bits(static_cast<int64_t>(constant_values_[loc.orig_sreg]));
  }

  bool IsConstantNullRef(RegLocation loc) const {
    return loc.ref && loc.is_const && (ConstantValue(loc) == 0);
  }

  int GetNumSSARegs() const {
    return num_ssa_regs_;
  }

  void SetNumSSARegs(int new_num) {
    num_ssa_regs_ = new_num;
  }

  unsigned int GetNumReachableBlocks() const {
    return num_reachable_blocks_;
  }

  int GetUseCount(int vreg) const {
    return use_counts_.Get(vreg);
  }

  int GetRawUseCount(int vreg) const {
    return raw_use_counts_.Get(vreg);
  }

  int GetSSASubscript(int ssa_reg) const {
    return ssa_subscripts_->Get(ssa_reg);
  }

  RegLocation GetRawSrc(MIR* mir, int num) {
    DCHECK(num < mir->ssa_rep->num_uses);
    RegLocation res = reg_location_[mir->ssa_rep->uses[num]];
    return res;
  }

  RegLocation GetRawDest(MIR* mir) {
    DCHECK_GT(mir->ssa_rep->num_defs, 0);
    RegLocation res = reg_location_[mir->ssa_rep->defs[0]];
    return res;
  }

  RegLocation GetDest(MIR* mir) {
    RegLocation res = GetRawDest(mir);
    DCHECK(!res.wide);
    return res;
  }

  RegLocation GetSrc(MIR* mir, int num) {
    RegLocation res = GetRawSrc(mir, num);
    DCHECK(!res.wide);
    return res;
  }

  RegLocation GetDestWide(MIR* mir) {
    RegLocation res = GetRawDest(mir);
    DCHECK(res.wide);
    return res;
  }

  RegLocation GetSrcWide(MIR* mir, int low) {
    RegLocation res = GetRawSrc(mir, low);
    DCHECK(res.wide);
    return res;
  }

  RegLocation GetBadLoc() {
    return bad_loc;
  }

  int GetMethodSReg() {
    return method_sreg_;
  }

  bool MethodIsLeaf() {
    return attributes_ & METHOD_IS_LEAF;
  }

  RegLocation GetRegLocation(int index) {
    DCHECK((index >= 0) && (index > num_ssa_regs_));
    return reg_location_[index];
  }

  RegLocation GetMethodLoc() {
    return reg_location_[method_sreg_];
  }

  bool IsSpecialCase() {
    return special_case_ != kNoHandler;
  }

  SpecialCaseHandler GetSpecialCase() {
    return special_case_;
  }

  bool IsBackedge(BasicBlock* branch_bb, BasicBlock* target_bb) {
    return ((target_bb != NULL) && (target_bb->start_offset <= branch_bb->start_offset));
  }

  bool IsBackwardsBranch(BasicBlock* branch_bb) {
    return IsBackedge(branch_bb, branch_bb->taken) || IsBackedge(branch_bb, branch_bb->fall_through);
  }

  void BasicBlockCombine();
  void CodeLayout();
  void DumpCheckStats();
  void PropagateConstants();
  MIR* FindMoveResult(BasicBlock* bb, MIR* mir);
  int SRegToVReg(int ssa_reg) const;
  void VerifyDataflow();
  void MethodUseCount();
  void SSATransformation();
  void CheckForDominanceFrontier(BasicBlock* dom_bb, const BasicBlock* succ_bb);
  void NullCheckElimination();
  bool SetFp(int index, bool is_fp);
  bool SetCore(int index, bool is_core);
  bool SetRef(int index, bool is_ref);
  bool SetWide(int index, bool is_wide);
  bool SetHigh(int index, bool is_high);
  void AppendMIR(BasicBlock* bb, MIR* mir);
  void PrependMIR(BasicBlock* bb, MIR* mir);
  void InsertMIRAfter(BasicBlock* bb, MIR* current_mir, MIR* new_mir);
  char* GetDalvikDisassembly(const MIR* mir);
  void ReplaceSpecialChars(std::string& str);
  std::string GetSSAName(int ssa_reg);
  std::string GetSSANameWithConst(int ssa_reg, bool singles_only);
  void GetBlockName(BasicBlock* bb, char* name);
  const char* GetShortyFromTargetIdx(int);
  void DumpMIRGraph();
  CallInfo* NewMemCallInfo(BasicBlock* bb, MIR* mir, InvokeType type, bool is_range);
  BasicBlock* NewMemBB(BBType block_type, int block_id);

  /*
   * IsDebugBuild sanity check: keep track of the Dex PCs for catch entries so that later on
   * we can verify that all catch entries have native PC entries.
   */
  std::set<uint32_t> catches_;

  // TODO: make these private.
  RegLocation* reg_location_;                         // Map SSA names to location.
  GrowableArray<CompilerTemp*> compiler_temps_;
  SafeMap<unsigned int, unsigned int> block_id_map_;  // Block collapse lookup cache.

  static const int oat_data_flow_attributes_[kMirOpLast];
  static const char* extended_mir_op_names_[kMirOpLast - kMirOpFirst];
  static const uint32_t analysis_attributes_[kMirOpLast];

 private:
  int FindCommonParent(int block1, int block2);
  void ComputeSuccLineIn(ArenaBitVector* dest, const ArenaBitVector* src1,
                         const ArenaBitVector* src2);
  void HandleLiveInUse(ArenaBitVector* use_v, ArenaBitVector* def_v,
                       ArenaBitVector* live_in_v, int dalvik_reg_id);
  void HandleDef(ArenaBitVector* def_v, int dalvik_reg_id);
  void CompilerInitializeSSAConversion();
  bool DoSSAConversion(BasicBlock* bb);
  bool InvokeUsesMethodStar(MIR* mir);
  int ParseInsn(const uint16_t* code_ptr, DecodedInstruction* decoded_instruction);
  bool ContentIsInsn(const uint16_t* code_ptr);
  BasicBlock* SplitBlock(unsigned int code_offset, BasicBlock* orig_block,
                         BasicBlock** immed_pred_block_p);
  BasicBlock* FindBlock(unsigned int code_offset, bool split, bool create,
                        BasicBlock** immed_pred_block_p);
  void ProcessTryCatchBlocks();
  BasicBlock* ProcessCanBranch(BasicBlock* cur_block, MIR* insn, int cur_offset, int width,
                               int flags, const uint16_t* code_ptr, const uint16_t* code_end);
  void ProcessCanSwitch(BasicBlock* cur_block, MIR* insn, int cur_offset, int width, int flags);
  BasicBlock* ProcessCanThrow(BasicBlock* cur_block, MIR* insn, int cur_offset, int width,
                              int flags, ArenaBitVector* try_block_addr, const uint16_t* code_ptr,
                              const uint16_t* code_end);
  int AddNewSReg(int v_reg);
  void HandleSSAUse(int* uses, int dalvik_reg, int reg_index);
  void HandleSSADef(int* defs, int dalvik_reg, int reg_index);
  void DataFlowSSAFormat35C(MIR* mir);
  void DataFlowSSAFormat3RC(MIR* mir);
  bool FindLocalLiveIn(BasicBlock* bb);
  void ClearAllVisitedFlags();
  bool CountUses(struct BasicBlock* bb);
  bool InferTypeAndSize(BasicBlock* bb);
  bool VerifyPredInfo(BasicBlock* bb);
  BasicBlock* NeedsVisit(BasicBlock* bb);
  BasicBlock* NextUnvisitedSuccessor(BasicBlock* bb);
  void MarkPreOrder(BasicBlock* bb);
  void RecordDFSOrders(BasicBlock* bb);
  void ComputeDFSOrders();
  void ComputeDefBlockMatrix();
  void ComputeDomPostOrderTraversal(BasicBlock* bb);
  void ComputeDominators();
  void InsertPhiNodes();
  void DoDFSPreOrderSSARename(BasicBlock* block);
  void SetConstant(int32_t ssa_reg, int value);
  void SetConstantWide(int ssa_reg, int64_t value);
  int GetSSAUseCount(int s_reg);
  bool BasicBlockOpt(BasicBlock* bb);
  bool EliminateNullChecks(BasicBlock* bb);
  void NullCheckEliminationInit(BasicBlock* bb);
  bool BuildExtendedBBList(struct BasicBlock* bb);
  bool FillDefBlockMatrix(BasicBlock* bb);
  void InitializeDominationInfo(BasicBlock* bb);
  bool ComputeblockIDom(BasicBlock* bb);
  bool ComputeBlockDominators(BasicBlock* bb);
  bool SetDominators(BasicBlock* bb);
  bool ComputeBlockLiveIns(BasicBlock* bb);
  bool InsertPhiNodeOperands(BasicBlock* bb);
  bool ComputeDominanceFrontier(BasicBlock* bb);
  void DoConstantPropogation(BasicBlock* bb);
  void CountChecks(BasicBlock* bb);
  bool CombineBlocks(BasicBlock* bb);
  void AnalyzeBlock(BasicBlock* bb, struct MethodStats* stats);
  bool ComputeSkipCompilation(struct MethodStats* stats, bool skip_default);

  CompilationUnit* const cu_;
  GrowableArray<int>* ssa_base_vregs_;
  GrowableArray<int>* ssa_subscripts_;
  // Map original Dalvik virtual reg i to the current SSA name.
  int* vreg_to_ssa_map_;            // length == method->registers_size
  int* ssa_last_defs_;              // length == method->registers_size
  ArenaBitVector* is_constant_v_;   // length == num_ssa_reg
  int* constant_values_;            // length == num_ssa_reg
  // Use counts of ssa names.
  GrowableArray<uint32_t> use_counts_;      // Weighted by nesting depth
  GrowableArray<uint32_t> raw_use_counts_;  // Not weighted
  unsigned int num_reachable_blocks_;
  GrowableArray<int>* dfs_order_;
  GrowableArray<int>* dfs_post_order_;
  GrowableArray<int>* dom_post_order_traversal_;
  int* i_dom_list_;
  ArenaBitVector** def_block_matrix_;    // num_dalvik_register x num_blocks.
  ArenaBitVector* temp_block_v_;
  ArenaBitVector* temp_dalvik_register_v_;
  ArenaBitVector* temp_ssa_register_v_;  // num_ssa_regs.
  static const int kInvalidEntry = -1;
  GrowableArray<BasicBlock*> block_list_;
  ArenaBitVector* try_block_addr_;
  BasicBlock* entry_block_;
  BasicBlock* exit_block_;
  BasicBlock* cur_block_;
  int num_blocks_;
  const DexFile::CodeItem* current_code_item_;
  SafeMap<unsigned int, BasicBlock*> block_map_;  // FindBlock lookup cache.
  std::vector<DexCompilationUnit*> m_units_;     // List of methods included in this graph
  typedef std::pair<int, int> MIRLocation;       // Insert point, (m_unit_ index, offset)
  std::vector<MIRLocation> method_stack_;        // Include stack
  int current_method_;
  int current_offset_;
  int def_count_;                                // Used to estimate size of ssa name storage.
  int* opcode_count_;                            // Dex opcode coverage stats.
  int num_ssa_regs_;                             // Number of names following SSA transformation.
  std::vector<BasicBlock*> extended_basic_blocks_;  // Heads of block "traces".
  int method_sreg_;
  unsigned int attributes_;
  Checkstats* checkstats_;
  SpecialCaseHandler special_case_;
  ArenaAllocator* arena_;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_MIR_GRAPH_H_
