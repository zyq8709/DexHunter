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

#ifndef ART_COMPILER_DEX_QUICK_MIR_TO_LIR_H_
#define ART_COMPILER_DEX_QUICK_MIR_TO_LIR_H_

#include "invoke_type.h"
#include "compiled_method.h"
#include "dex/compiler_enums.h"
#include "dex/compiler_ir.h"
#include "dex/backend.h"
#include "dex/growable_array.h"
#include "dex/arena_allocator.h"
#include "driver/compiler_driver.h"
#include "leb128_encoder.h"
#include "safe_map.h"

namespace art {

// Set to 1 to measure cost of suspend check.
#define NO_SUSPEND 0

#define IS_BINARY_OP         (1ULL << kIsBinaryOp)
#define IS_BRANCH            (1ULL << kIsBranch)
#define IS_IT                (1ULL << kIsIT)
#define IS_LOAD              (1ULL << kMemLoad)
#define IS_QUAD_OP           (1ULL << kIsQuadOp)
#define IS_QUIN_OP           (1ULL << kIsQuinOp)
#define IS_SEXTUPLE_OP       (1ULL << kIsSextupleOp)
#define IS_STORE             (1ULL << kMemStore)
#define IS_TERTIARY_OP       (1ULL << kIsTertiaryOp)
#define IS_UNARY_OP          (1ULL << kIsUnaryOp)
#define NEEDS_FIXUP          (1ULL << kPCRelFixup)
#define NO_OPERAND           (1ULL << kNoOperand)
#define REG_DEF0             (1ULL << kRegDef0)
#define REG_DEF1             (1ULL << kRegDef1)
#define REG_DEFA             (1ULL << kRegDefA)
#define REG_DEFD             (1ULL << kRegDefD)
#define REG_DEF_FPCS_LIST0   (1ULL << kRegDefFPCSList0)
#define REG_DEF_FPCS_LIST2   (1ULL << kRegDefFPCSList2)
#define REG_DEF_LIST0        (1ULL << kRegDefList0)
#define REG_DEF_LIST1        (1ULL << kRegDefList1)
#define REG_DEF_LR           (1ULL << kRegDefLR)
#define REG_DEF_SP           (1ULL << kRegDefSP)
#define REG_USE0             (1ULL << kRegUse0)
#define REG_USE1             (1ULL << kRegUse1)
#define REG_USE2             (1ULL << kRegUse2)
#define REG_USE3             (1ULL << kRegUse3)
#define REG_USE4             (1ULL << kRegUse4)
#define REG_USEA             (1ULL << kRegUseA)
#define REG_USEC             (1ULL << kRegUseC)
#define REG_USED             (1ULL << kRegUseD)
#define REG_USE_FPCS_LIST0   (1ULL << kRegUseFPCSList0)
#define REG_USE_FPCS_LIST2   (1ULL << kRegUseFPCSList2)
#define REG_USE_LIST0        (1ULL << kRegUseList0)
#define REG_USE_LIST1        (1ULL << kRegUseList1)
#define REG_USE_LR           (1ULL << kRegUseLR)
#define REG_USE_PC           (1ULL << kRegUsePC)
#define REG_USE_SP           (1ULL << kRegUseSP)
#define SETS_CCODES          (1ULL << kSetsCCodes)
#define USES_CCODES          (1ULL << kUsesCCodes)

// Common combo register usage patterns.
#define REG_DEF01            (REG_DEF0 | REG_DEF1)
#define REG_DEF01_USE2       (REG_DEF0 | REG_DEF1 | REG_USE2)
#define REG_DEF0_USE01       (REG_DEF0 | REG_USE01)
#define REG_DEF0_USE0        (REG_DEF0 | REG_USE0)
#define REG_DEF0_USE12       (REG_DEF0 | REG_USE12)
#define REG_DEF0_USE1        (REG_DEF0 | REG_USE1)
#define REG_DEF0_USE2        (REG_DEF0 | REG_USE2)
#define REG_DEFAD_USEAD      (REG_DEFAD_USEA | REG_USED)
#define REG_DEFAD_USEA       (REG_DEFA_USEA | REG_DEFD)
#define REG_DEFA_USEA        (REG_DEFA | REG_USEA)
#define REG_USE012           (REG_USE01 | REG_USE2)
#define REG_USE014           (REG_USE01 | REG_USE4)
#define REG_USE01            (REG_USE0 | REG_USE1)
#define REG_USE02            (REG_USE0 | REG_USE2)
#define REG_USE12            (REG_USE1 | REG_USE2)
#define REG_USE23            (REG_USE2 | REG_USE3)

struct BasicBlock;
struct CallInfo;
struct CompilationUnit;
struct MIR;
struct RegLocation;
struct RegisterInfo;
class MIRGraph;
class Mir2Lir;

typedef int (*NextCallInsn)(CompilationUnit*, CallInfo*, int,
                            const MethodReference& target_method,
                            uint32_t method_idx, uintptr_t direct_code,
                            uintptr_t direct_method, InvokeType type);

typedef std::vector<uint8_t> CodeBuffer;


struct LIR {
  int offset;               // Offset of this instruction.
  int dalvik_offset;        // Offset of Dalvik opcode.
  LIR* next;
  LIR* prev;
  LIR* target;
  int opcode;
  int operands[5];          // [0..4] = [dest, src1, src2, extra, extra2].
  struct {
    bool is_nop:1;          // LIR is optimized away.
    bool pcRelFixup:1;      // May need pc-relative fixup.
    unsigned int size:5;    // Note: size is in bytes.
    unsigned int unused:25;
  } flags;
  int alias_info;           // For Dalvik register & litpool disambiguation.
  uint64_t use_mask;        // Resource mask for use.
  uint64_t def_mask;        // Resource mask for def.
};

// Target-specific initialization.
Mir2Lir* ArmCodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                          ArenaAllocator* const arena);
Mir2Lir* MipsCodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                          ArenaAllocator* const arena);
Mir2Lir* X86CodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                          ArenaAllocator* const arena);

// Utility macros to traverse the LIR list.
#define NEXT_LIR(lir) (lir->next)
#define PREV_LIR(lir) (lir->prev)

// Defines for alias_info (tracks Dalvik register references).
#define DECODE_ALIAS_INFO_REG(X)        (X & 0xffff)
#define DECODE_ALIAS_INFO_WIDE_FLAG     (0x80000000)
#define DECODE_ALIAS_INFO_WIDE(X)       ((X & DECODE_ALIAS_INFO_WIDE_FLAG) ? 1 : 0)
#define ENCODE_ALIAS_INFO(REG, ISWIDE)  (REG | (ISWIDE ? DECODE_ALIAS_INFO_WIDE_FLAG : 0))

// Common resource macros.
#define ENCODE_CCODE            (1ULL << kCCode)
#define ENCODE_FP_STATUS        (1ULL << kFPStatus)

// Abstract memory locations.
#define ENCODE_DALVIK_REG       (1ULL << kDalvikReg)
#define ENCODE_LITERAL          (1ULL << kLiteral)
#define ENCODE_HEAP_REF         (1ULL << kHeapRef)
#define ENCODE_MUST_NOT_ALIAS   (1ULL << kMustNotAlias)

#define ENCODE_ALL              (~0ULL)
#define ENCODE_MEM              (ENCODE_DALVIK_REG | ENCODE_LITERAL | \
                                 ENCODE_HEAP_REF | ENCODE_MUST_NOT_ALIAS)
// TODO: replace these macros
#define SLOW_FIELD_PATH (cu_->enable_debug & (1 << kDebugSlowFieldPath))
#define SLOW_INVOKE_PATH (cu_->enable_debug & (1 << kDebugSlowInvokePath))
#define SLOW_STRING_PATH (cu_->enable_debug & (1 << kDebugSlowStringPath))
#define SLOW_TYPE_PATH (cu_->enable_debug & (1 << kDebugSlowTypePath))
#define EXERCISE_SLOWEST_STRING_PATH (cu_->enable_debug & (1 << kDebugSlowestStringPath))
#define is_pseudo_opcode(opcode) (static_cast<int>(opcode) < 0)

class Mir2Lir : public Backend {
  public:
    struct SwitchTable {
      int offset;
      const uint16_t* table;      // Original dex table.
      int vaddr;                  // Dalvik offset of switch opcode.
      LIR* anchor;                // Reference instruction for relative offsets.
      LIR** targets;              // Array of case targets.
    };

    struct FillArrayData {
      int offset;
      const uint16_t* table;      // Original dex table.
      int size;
      int vaddr;                  // Dalvik offset of FILL_ARRAY_DATA opcode.
    };

    /* Static register use counts */
    struct RefCounts {
      int count;
      int s_reg;
      bool double_start;   // Starting v_reg for a double
    };

    /*
     * Data structure tracking the mapping between a Dalvik register (pair) and a
     * native register (pair). The idea is to reuse the previously loaded value
     * if possible, otherwise to keep the value in a native register as long as
     * possible.
     */
    struct RegisterInfo {
      int reg;                    // Reg number
      bool in_use;                // Has it been allocated?
      bool is_temp;               // Can allocate as temp?
      bool pair;                  // Part of a register pair?
      int partner;                // If pair, other reg of pair.
      bool live;                  // Is there an associated SSA name?
      bool dirty;                 // If live, is it dirty?
      int s_reg;                  // Name of live value.
      LIR *def_start;             // Starting inst in last def sequence.
      LIR *def_end;               // Ending inst in last def sequence.
    };

    struct RegisterPool {
       int num_core_regs;
       RegisterInfo *core_regs;
       int next_core_reg;
       int num_fp_regs;
       RegisterInfo *FPRegs;
       int next_fp_reg;
     };

    struct PromotionMap {
      RegLocationType core_location:3;
      uint8_t core_reg;
      RegLocationType fp_location:3;
      uint8_t FpReg;
      bool first_in_pair;
    };

    virtual ~Mir2Lir() {}

    int32_t s4FromSwitchData(const void* switch_data) {
      return *reinterpret_cast<const int32_t*>(switch_data);
    }

    RegisterClass oat_reg_class_by_size(OpSize size) {
      return (size == kUnsignedHalf || size == kSignedHalf || size == kUnsignedByte ||
              size == kSignedByte) ? kCoreReg : kAnyReg;
    }

    size_t CodeBufferSizeInBytes() {
      return code_buffer_.size() / sizeof(code_buffer_[0]);
    }

    // Shared by all targets - implemented in codegen_util.cc
    void AppendLIR(LIR* lir);
    void InsertLIRBefore(LIR* current_lir, LIR* new_lir);
    void InsertLIRAfter(LIR* current_lir, LIR* new_lir);

    int ComputeFrameSize();
    virtual void Materialize();
    virtual CompiledMethod* GetCompiledMethod();
    void MarkSafepointPC(LIR* inst);
    bool FastInstance(uint32_t field_idx, int& field_offset, bool& is_volatile, bool is_put);
    void SetupResourceMasks(LIR* lir);
    void AssembleLIR();
    void SetMemRefType(LIR* lir, bool is_load, int mem_type);
    void AnnotateDalvikRegAccess(LIR* lir, int reg_id, bool is_load, bool is64bit);
    void SetupRegMask(uint64_t* mask, int reg);
    void DumpLIRInsn(LIR* arg, unsigned char* base_addr);
    void DumpPromotionMap();
    void CodegenDump();
    LIR* RawLIR(int dalvik_offset, int opcode, int op0 = 0, int op1 = 0,
                int op2 = 0, int op3 = 0, int op4 = 0, LIR* target = NULL);
    LIR* NewLIR0(int opcode);
    LIR* NewLIR1(int opcode, int dest);
    LIR* NewLIR2(int opcode, int dest, int src1);
    LIR* NewLIR3(int opcode, int dest, int src1, int src2);
    LIR* NewLIR4(int opcode, int dest, int src1, int src2, int info);
    LIR* NewLIR5(int opcode, int dest, int src1, int src2, int info1, int info2);
    LIR* ScanLiteralPool(LIR* data_target, int value, unsigned int delta);
    LIR* ScanLiteralPoolWide(LIR* data_target, int val_lo, int val_hi);
    LIR* AddWordData(LIR* *constant_list_p, int value);
    LIR* AddWideData(LIR* *constant_list_p, int val_lo, int val_hi);
    void ProcessSwitchTables();
    void DumpSparseSwitchTable(const uint16_t* table);
    void DumpPackedSwitchTable(const uint16_t* table);
    LIR* MarkBoundary(int offset, const char* inst_str);
    void NopLIR(LIR* lir);
    bool EvaluateBranch(Instruction::Code opcode, int src1, int src2);
    bool IsInexpensiveConstant(RegLocation rl_src);
    ConditionCode FlipComparisonOrder(ConditionCode before);
    void DumpMappingTable(const char* table_name, const std::string& descriptor,
                          const std::string& name, const std::string& signature,
                          const std::vector<uint32_t>& v);
    void InstallLiteralPools();
    void InstallSwitchTables();
    void InstallFillArrayData();
    bool VerifyCatchEntries();
    void CreateMappingTables();
    void CreateNativeGcMap();
    int AssignLiteralOffset(int offset);
    int AssignSwitchTablesOffset(int offset);
    int AssignFillArrayDataOffset(int offset);
    int AssignInsnOffsets();
    void AssignOffsets();
    LIR* InsertCaseLabel(int vaddr, int keyVal);
    void MarkPackedCaseLabels(Mir2Lir::SwitchTable *tab_rec);
    void MarkSparseCaseLabels(Mir2Lir::SwitchTable *tab_rec);

    // Shared by all targets - implemented in local_optimizations.cc
    void ConvertMemOpIntoMove(LIR* orig_lir, int dest, int src);
    void ApplyLoadStoreElimination(LIR* head_lir, LIR* tail_lir);
    void ApplyLoadHoisting(LIR* head_lir, LIR* tail_lir);
    void ApplyLocalOptimizations(LIR* head_lir, LIR* tail_lir);
    void RemoveRedundantBranches();

    // Shared by all targets - implemented in ralloc_util.cc
    int GetSRegHi(int lowSreg);
    bool oat_live_out(int s_reg);
    int oatSSASrc(MIR* mir, int num);
    void SimpleRegAlloc();
    void ResetRegPool();
    void CompilerInitPool(RegisterInfo* regs, int* reg_nums, int num);
    void DumpRegPool(RegisterInfo* p, int num_regs);
    void DumpCoreRegPool();
    void DumpFpRegPool();
    /* Mark a temp register as dead.  Does not affect allocation state. */
    void Clobber(int reg) {
      ClobberBody(GetRegInfo(reg));
    }
    void ClobberSRegBody(RegisterInfo* p, int num_regs, int s_reg);
    void ClobberSReg(int s_reg);
    int SRegToPMap(int s_reg);
    void RecordCorePromotion(int reg, int s_reg);
    int AllocPreservedCoreReg(int s_reg);
    void RecordFpPromotion(int reg, int s_reg);
    int AllocPreservedSingle(int s_reg, bool even);
    int AllocPreservedDouble(int s_reg);
    int AllocPreservedFPReg(int s_reg, bool double_start);
    int AllocTempBody(RegisterInfo* p, int num_regs, int* next_temp,
                      bool required);
    int AllocTempDouble();
    int AllocFreeTemp();
    int AllocTemp();
    int AllocTempFloat();
    RegisterInfo* AllocLiveBody(RegisterInfo* p, int num_regs, int s_reg);
    RegisterInfo* AllocLive(int s_reg, int reg_class);
    void FreeTemp(int reg);
    RegisterInfo* IsLive(int reg);
    RegisterInfo* IsTemp(int reg);
    RegisterInfo* IsPromoted(int reg);
    bool IsDirty(int reg);
    void LockTemp(int reg);
    void ResetDef(int reg);
    void NullifyRange(LIR *start, LIR *finish, int s_reg1, int s_reg2);
    void MarkDef(RegLocation rl, LIR *start, LIR *finish);
    void MarkDefWide(RegLocation rl, LIR *start, LIR *finish);
    RegLocation WideToNarrow(RegLocation rl);
    void ResetDefLoc(RegLocation rl);
    void ResetDefLocWide(RegLocation rl);
    void ResetDefTracking();
    void ClobberAllRegs();
    void FlushAllRegsBody(RegisterInfo* info, int num_regs);
    void FlushAllRegs();
    bool RegClassMatches(int reg_class, int reg);
    void MarkLive(int reg, int s_reg);
    void MarkTemp(int reg);
    void UnmarkTemp(int reg);
    void MarkPair(int low_reg, int high_reg);
    void MarkClean(RegLocation loc);
    void MarkDirty(RegLocation loc);
    void MarkInUse(int reg);
    void CopyRegInfo(int new_reg, int old_reg);
    bool CheckCorePoolSanity();
    RegLocation UpdateLoc(RegLocation loc);
    RegLocation UpdateLocWide(RegLocation loc);
    RegLocation UpdateRawLoc(RegLocation loc);
    RegLocation EvalLocWide(RegLocation loc, int reg_class, bool update);
    RegLocation EvalLoc(RegLocation loc, int reg_class, bool update);
    void CountRefs(RefCounts* core_counts, RefCounts* fp_counts);
    void DumpCounts(const RefCounts* arr, int size, const char* msg);
    void DoPromotion();
    int VRegOffset(int v_reg);
    int SRegOffset(int s_reg);
    RegLocation GetReturnWide(bool is_double);
    RegLocation GetReturn(bool is_float);

    // Shared by all targets - implemented in gen_common.cc.
    bool HandleEasyDivRem(Instruction::Code dalvik_opcode, bool is_div,
                          RegLocation rl_src, RegLocation rl_dest, int lit);
    bool HandleEasyMultiply(RegLocation rl_src, RegLocation rl_dest, int lit);
    void HandleSuspendLaunchPads();
    void HandleIntrinsicLaunchPads();
    void HandleThrowLaunchPads();
    void GenBarrier();
    LIR* GenCheck(ConditionCode c_code, ThrowKind kind);
    LIR* GenImmedCheck(ConditionCode c_code, int reg, int imm_val,
                       ThrowKind kind);
    LIR* GenNullCheck(int s_reg, int m_reg, int opt_flags);
    LIR* GenRegRegCheck(ConditionCode c_code, int reg1, int reg2,
                        ThrowKind kind);
    void GenCompareAndBranch(Instruction::Code opcode, RegLocation rl_src1,
                             RegLocation rl_src2, LIR* taken, LIR* fall_through);
    void GenCompareZeroAndBranch(Instruction::Code opcode, RegLocation rl_src,
                                 LIR* taken, LIR* fall_through);
    void GenIntToLong(RegLocation rl_dest, RegLocation rl_src);
    void GenIntNarrowing(Instruction::Code opcode, RegLocation rl_dest,
                         RegLocation rl_src);
    void GenNewArray(uint32_t type_idx, RegLocation rl_dest,
                     RegLocation rl_src);
    void GenFilledNewArray(CallInfo* info);
    void GenSput(uint32_t field_idx, RegLocation rl_src,
                 bool is_long_or_double, bool is_object);
    void GenSget(uint32_t field_idx, RegLocation rl_dest,
                 bool is_long_or_double, bool is_object);
    void GenIGet(uint32_t field_idx, int opt_flags, OpSize size,
                 RegLocation rl_dest, RegLocation rl_obj, bool is_long_or_double, bool is_object);
    void GenIPut(uint32_t field_idx, int opt_flags, OpSize size,
                 RegLocation rl_src, RegLocation rl_obj, bool is_long_or_double, bool is_object);
    void GenConstClass(uint32_t type_idx, RegLocation rl_dest);
    void GenConstString(uint32_t string_idx, RegLocation rl_dest);
    void GenNewInstance(uint32_t type_idx, RegLocation rl_dest);
    void GenThrow(RegLocation rl_src);
    void GenInstanceof(uint32_t type_idx, RegLocation rl_dest,
                       RegLocation rl_src);
    void GenCheckCast(uint32_t insn_idx, uint32_t type_idx,
                      RegLocation rl_src);
    void GenLong3Addr(OpKind first_op, OpKind second_op, RegLocation rl_dest,
                      RegLocation rl_src1, RegLocation rl_src2);
    void GenShiftOpLong(Instruction::Code opcode, RegLocation rl_dest,
                        RegLocation rl_src1, RegLocation rl_shift);
    void GenArithOpInt(Instruction::Code opcode, RegLocation rl_dest,
                       RegLocation rl_src1, RegLocation rl_src2);
    void GenArithOpIntLit(Instruction::Code opcode, RegLocation rl_dest,
                          RegLocation rl_src, int lit);
    void GenArithOpLong(Instruction::Code opcode, RegLocation rl_dest,
                        RegLocation rl_src1, RegLocation rl_src2);
    void GenConversionCall(ThreadOffset func_offset, RegLocation rl_dest,
                           RegLocation rl_src);
    void GenSuspendTest(int opt_flags);
    void GenSuspendTestAndBranch(int opt_flags, LIR* target);

    // Shared by all targets - implemented in gen_invoke.cc.
    int CallHelperSetup(ThreadOffset helper_offset);
    LIR* CallHelper(int r_tgt, ThreadOffset helper_offset, bool safepoint_pc, bool use_link = true);
    void CallRuntimeHelperImm(ThreadOffset helper_offset, int arg0, bool safepoint_pc);
    void CallRuntimeHelperReg(ThreadOffset helper_offset, int arg0, bool safepoint_pc);
    void CallRuntimeHelperRegLocation(ThreadOffset helper_offset, RegLocation arg0,
                                      bool safepoint_pc);
    void CallRuntimeHelperImmImm(ThreadOffset helper_offset, int arg0, int arg1,
                                 bool safepoint_pc);
    void CallRuntimeHelperImmRegLocation(ThreadOffset helper_offset, int arg0,
                                         RegLocation arg1, bool safepoint_pc);
    void CallRuntimeHelperRegLocationImm(ThreadOffset helper_offset, RegLocation arg0,
                                         int arg1, bool safepoint_pc);
    void CallRuntimeHelperImmReg(ThreadOffset helper_offset, int arg0, int arg1,
                                 bool safepoint_pc);
    void CallRuntimeHelperRegImm(ThreadOffset helper_offset, int arg0, int arg1,
                                 bool safepoint_pc);
    void CallRuntimeHelperImmMethod(ThreadOffset helper_offset, int arg0,
                                    bool safepoint_pc);
    void CallRuntimeHelperRegLocationRegLocation(ThreadOffset helper_offset,
                                                 RegLocation arg0, RegLocation arg1,
                                                 bool safepoint_pc);
    void CallRuntimeHelperRegReg(ThreadOffset helper_offset, int arg0, int arg1,
                                 bool safepoint_pc);
    void CallRuntimeHelperRegRegImm(ThreadOffset helper_offset, int arg0, int arg1,
                                    int arg2, bool safepoint_pc);
    void CallRuntimeHelperImmMethodRegLocation(ThreadOffset helper_offset, int arg0,
                                               RegLocation arg2, bool safepoint_pc);
    void CallRuntimeHelperImmMethodImm(ThreadOffset helper_offset, int arg0, int arg2,
                                       bool safepoint_pc);
    void CallRuntimeHelperImmRegLocationRegLocation(ThreadOffset helper_offset,
                                                    int arg0, RegLocation arg1, RegLocation arg2,
                                                    bool safepoint_pc);
    void GenInvoke(CallInfo* info);
    void FlushIns(RegLocation* ArgLocs, RegLocation rl_method);
    int GenDalvikArgsNoRange(CallInfo* info, int call_state, LIR** pcrLabel,
                             NextCallInsn next_call_insn,
                             const MethodReference& target_method,
                             uint32_t vtable_idx,
                             uintptr_t direct_code, uintptr_t direct_method, InvokeType type,
                             bool skip_this);
    int GenDalvikArgsRange(CallInfo* info, int call_state, LIR** pcrLabel,
                           NextCallInsn next_call_insn,
                           const MethodReference& target_method,
                           uint32_t vtable_idx,
                           uintptr_t direct_code, uintptr_t direct_method, InvokeType type,
                           bool skip_this);
    RegLocation InlineTarget(CallInfo* info);
    RegLocation InlineTargetWide(CallInfo* info);

    bool GenInlinedCharAt(CallInfo* info);
    bool GenInlinedStringIsEmptyOrLength(CallInfo* info, bool is_empty);
    bool GenInlinedAbsInt(CallInfo* info);
    bool GenInlinedAbsLong(CallInfo* info);
    bool GenInlinedFloatCvt(CallInfo* info);
    bool GenInlinedDoubleCvt(CallInfo* info);
    bool GenInlinedIndexOf(CallInfo* info, bool zero_based);
    bool GenInlinedStringCompareTo(CallInfo* info);
    bool GenInlinedCurrentThread(CallInfo* info);
    bool GenInlinedUnsafeGet(CallInfo* info, bool is_long, bool is_volatile);
    bool GenInlinedUnsafePut(CallInfo* info, bool is_long, bool is_object,
                             bool is_volatile, bool is_ordered);
    bool GenIntrinsic(CallInfo* info);
    int LoadArgRegs(CallInfo* info, int call_state,
                    NextCallInsn next_call_insn,
                    const MethodReference& target_method,
                    uint32_t vtable_idx,
                    uintptr_t direct_code, uintptr_t direct_method, InvokeType type,
                    bool skip_this);

    // Shared by all targets - implemented in gen_loadstore.cc.
    RegLocation LoadCurrMethod();
    void LoadCurrMethodDirect(int r_tgt);
    LIR* LoadConstant(int r_dest, int value);
    LIR* LoadWordDisp(int rBase, int displacement, int r_dest);
    RegLocation LoadValue(RegLocation rl_src, RegisterClass op_kind);
    RegLocation LoadValueWide(RegLocation rl_src, RegisterClass op_kind);
    void LoadValueDirect(RegLocation rl_src, int r_dest);
    void LoadValueDirectFixed(RegLocation rl_src, int r_dest);
    void LoadValueDirectWide(RegLocation rl_src, int reg_lo, int reg_hi);
    void LoadValueDirectWideFixed(RegLocation rl_src, int reg_lo, int reg_hi);
    LIR* StoreWordDisp(int rBase, int displacement, int r_src);
    void StoreValue(RegLocation rl_dest, RegLocation rl_src);
    void StoreValueWide(RegLocation rl_dest, RegLocation rl_src);

    // Shared by all targets - implemented in mir_to_lir.cc.
    void CompileDalvikInstruction(MIR* mir, BasicBlock* bb, LIR* label_list);
    void HandleExtendedMethodMIR(BasicBlock* bb, MIR* mir);
    bool MethodBlockCodeGen(BasicBlock* bb);
    void SpecialMIR2LIR(SpecialCaseHandler special_case);
    void MethodMIR2LIR();



    // Required for target - codegen helpers.
    virtual bool SmallLiteralDivRem(Instruction::Code dalvik_opcode, bool is_div,
                                    RegLocation rl_src, RegLocation rl_dest, int lit) = 0;
    virtual int LoadHelper(ThreadOffset offset) = 0;
    virtual LIR* LoadBaseDisp(int rBase, int displacement, int r_dest, OpSize size, int s_reg) = 0;
    virtual LIR* LoadBaseDispWide(int rBase, int displacement, int r_dest_lo, int r_dest_hi,
                                  int s_reg) = 0;
    virtual LIR* LoadBaseIndexed(int rBase, int r_index, int r_dest, int scale, OpSize size) = 0;
    virtual LIR* LoadBaseIndexedDisp(int rBase, int r_index, int scale, int displacement,
                                     int r_dest, int r_dest_hi, OpSize size, int s_reg) = 0;
    virtual LIR* LoadConstantNoClobber(int r_dest, int value) = 0;
    virtual LIR* LoadConstantWide(int r_dest_lo, int r_dest_hi, int64_t value) = 0;
    virtual LIR* StoreBaseDisp(int rBase, int displacement, int r_src, OpSize size) = 0;
    virtual LIR* StoreBaseDispWide(int rBase, int displacement, int r_src_lo, int r_src_hi) = 0;
    virtual LIR* StoreBaseIndexed(int rBase, int r_index, int r_src, int scale, OpSize size) = 0;
    virtual LIR* StoreBaseIndexedDisp(int rBase, int r_index, int scale, int displacement,
                                      int r_src, int r_src_hi, OpSize size, int s_reg) = 0;
    virtual void MarkGCCard(int val_reg, int tgt_addr_reg) = 0;

    // Required for target - register utilities.
    virtual bool IsFpReg(int reg) = 0;
    virtual bool SameRegType(int reg1, int reg2) = 0;
    virtual int AllocTypedTemp(bool fp_hint, int reg_class) = 0;
    virtual int AllocTypedTempPair(bool fp_hint, int reg_class) = 0;
    virtual int S2d(int low_reg, int high_reg) = 0;
    virtual int TargetReg(SpecialTargetRegister reg) = 0;
    virtual RegisterInfo* GetRegInfo(int reg) = 0;
    virtual RegLocation GetReturnAlt() = 0;
    virtual RegLocation GetReturnWideAlt() = 0;
    virtual RegLocation LocCReturn() = 0;
    virtual RegLocation LocCReturnDouble() = 0;
    virtual RegLocation LocCReturnFloat() = 0;
    virtual RegLocation LocCReturnWide() = 0;
    virtual uint32_t FpRegMask() = 0;
    virtual uint64_t GetRegMaskCommon(int reg) = 0;
    virtual void AdjustSpillMask() = 0;
    virtual void ClobberCalleeSave() = 0;
    virtual void FlushReg(int reg) = 0;
    virtual void FlushRegWide(int reg1, int reg2) = 0;
    virtual void FreeCallTemps() = 0;
    virtual void FreeRegLocTemps(RegLocation rl_keep, RegLocation rl_free) = 0;
    virtual void LockCallTemps() = 0;
    virtual void MarkPreservedSingle(int v_reg, int reg) = 0;
    virtual void CompilerInitializeRegAlloc() = 0;

    // Required for target - miscellaneous.
    virtual AssemblerStatus AssembleInstructions(uintptr_t start_addr) = 0;
    virtual void DumpResourceMask(LIR* lir, uint64_t mask, const char* prefix) = 0;
    virtual void SetupTargetResourceMasks(LIR* lir) = 0;
    virtual const char* GetTargetInstFmt(int opcode) = 0;
    virtual const char* GetTargetInstName(int opcode) = 0;
    virtual std::string BuildInsnString(const char* fmt, LIR* lir, unsigned char* base_addr) = 0;
    virtual uint64_t GetPCUseDefEncoding() = 0;
    virtual uint64_t GetTargetInstFlags(int opcode) = 0;
    virtual int GetInsnSize(LIR* lir) = 0;
    virtual bool IsUnconditionalBranch(LIR* lir) = 0;

    // Required for target - Dalvik-level generators.
    virtual void GenArithImmOpLong(Instruction::Code opcode, RegLocation rl_dest,
                                   RegLocation rl_src1, RegLocation rl_src2) = 0;
    virtual void GenMulLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) = 0;
    virtual void GenAddLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) = 0;
    virtual void GenAndLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) = 0;
    virtual void GenArithOpDouble(Instruction::Code opcode,
                                  RegLocation rl_dest, RegLocation rl_src1,
                                  RegLocation rl_src2) = 0;
    virtual void GenArithOpFloat(Instruction::Code opcode, RegLocation rl_dest,
                                 RegLocation rl_src1, RegLocation rl_src2) = 0;
    virtual void GenCmpFP(Instruction::Code opcode, RegLocation rl_dest,
                          RegLocation rl_src1, RegLocation rl_src2) = 0;
    virtual void GenConversion(Instruction::Code opcode, RegLocation rl_dest,
                               RegLocation rl_src) = 0;
    virtual bool GenInlinedCas32(CallInfo* info, bool need_write_barrier) = 0;
    virtual bool GenInlinedMinMaxInt(CallInfo* info, bool is_min) = 0;
    virtual bool GenInlinedSqrt(CallInfo* info) = 0;
    virtual void GenNegLong(RegLocation rl_dest, RegLocation rl_src) = 0;
    virtual void GenOrLong(RegLocation rl_dest, RegLocation rl_src1,
                           RegLocation rl_src2) = 0;
    virtual void GenSubLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) = 0;
    virtual void GenXorLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) = 0;
    virtual LIR* GenRegMemCheck(ConditionCode c_code, int reg1, int base,
                                int offset, ThrowKind kind) = 0;
    virtual RegLocation GenDivRem(RegLocation rl_dest, int reg_lo, int reg_hi,
                                  bool is_div) = 0;
    virtual RegLocation GenDivRemLit(RegLocation rl_dest, int reg_lo, int lit,
                                     bool is_div) = 0;
    virtual void GenCmpLong(RegLocation rl_dest, RegLocation rl_src1,
                            RegLocation rl_src2) = 0;
    virtual void GenDivZeroCheck(int reg_lo, int reg_hi) = 0;
    virtual void GenEntrySequence(RegLocation* ArgLocs,
                                  RegLocation rl_method) = 0;
    virtual void GenExitSequence() = 0;
    virtual void GenFillArrayData(uint32_t table_offset,
                                  RegLocation rl_src) = 0;
    virtual void GenFusedFPCmpBranch(BasicBlock* bb, MIR* mir, bool gt_bias,
                                     bool is_double) = 0;
    virtual void GenFusedLongCmpBranch(BasicBlock* bb, MIR* mir) = 0;
    virtual void GenSelect(BasicBlock* bb, MIR* mir) = 0;
    virtual void GenMemBarrier(MemBarrierKind barrier_kind) = 0;
    virtual void GenMonitorEnter(int opt_flags, RegLocation rl_src) = 0;
    virtual void GenMonitorExit(int opt_flags, RegLocation rl_src) = 0;
    virtual void GenMoveException(RegLocation rl_dest) = 0;
    virtual void GenMultiplyByTwoBitMultiplier(RegLocation rl_src,
                                               RegLocation rl_result, int lit, int first_bit,
                                               int second_bit) = 0;
    virtual void GenNegDouble(RegLocation rl_dest, RegLocation rl_src) = 0;
    virtual void GenNegFloat(RegLocation rl_dest, RegLocation rl_src) = 0;
    virtual void GenPackedSwitch(MIR* mir, uint32_t table_offset,
                                 RegLocation rl_src) = 0;
    virtual void GenSparseSwitch(MIR* mir, uint32_t table_offset,
                                 RegLocation rl_src) = 0;
    virtual void GenSpecialCase(BasicBlock* bb, MIR* mir,
                                SpecialCaseHandler special_case) = 0;
    virtual void GenArrayObjPut(int opt_flags, RegLocation rl_array,
                                RegLocation rl_index, RegLocation rl_src, int scale) = 0;
    virtual void GenArrayGet(int opt_flags, OpSize size, RegLocation rl_array,
                             RegLocation rl_index, RegLocation rl_dest, int scale) = 0;
    virtual void GenArrayPut(int opt_flags, OpSize size, RegLocation rl_array,
                     RegLocation rl_index, RegLocation rl_src, int scale) = 0;
    virtual void GenShiftImmOpLong(Instruction::Code opcode,
                                   RegLocation rl_dest, RegLocation rl_src1,
                                   RegLocation rl_shift) = 0;

    // Required for target - single operation generators.
    virtual LIR* OpUnconditionalBranch(LIR* target) = 0;
    virtual LIR* OpCmpBranch(ConditionCode cond, int src1, int src2,
                             LIR* target) = 0;
    virtual LIR* OpCmpImmBranch(ConditionCode cond, int reg, int check_value,
                                LIR* target) = 0;
    virtual LIR* OpCondBranch(ConditionCode cc, LIR* target) = 0;
    virtual LIR* OpDecAndBranch(ConditionCode c_code, int reg,
                                LIR* target) = 0;
    virtual LIR* OpFpRegCopy(int r_dest, int r_src) = 0;
    virtual LIR* OpIT(ConditionCode cond, const char* guide) = 0;
    virtual LIR* OpMem(OpKind op, int rBase, int disp) = 0;
    virtual LIR* OpPcRelLoad(int reg, LIR* target) = 0;
    virtual LIR* OpReg(OpKind op, int r_dest_src) = 0;
    virtual LIR* OpRegCopy(int r_dest, int r_src) = 0;
    virtual LIR* OpRegCopyNoInsert(int r_dest, int r_src) = 0;
    virtual LIR* OpRegImm(OpKind op, int r_dest_src1, int value) = 0;
    virtual LIR* OpRegMem(OpKind op, int r_dest, int rBase, int offset) = 0;
    virtual LIR* OpRegReg(OpKind op, int r_dest_src1, int r_src2) = 0;
    virtual LIR* OpRegRegImm(OpKind op, int r_dest, int r_src1, int value) = 0;
    virtual LIR* OpRegRegReg(OpKind op, int r_dest, int r_src1,
                             int r_src2) = 0;
    virtual LIR* OpTestSuspend(LIR* target) = 0;
    virtual LIR* OpThreadMem(OpKind op, ThreadOffset thread_offset) = 0;
    virtual LIR* OpVldm(int rBase, int count) = 0;
    virtual LIR* OpVstm(int rBase, int count) = 0;
    virtual void OpLea(int rBase, int reg1, int reg2, int scale,
                       int offset) = 0;
    virtual void OpRegCopyWide(int dest_lo, int dest_hi, int src_lo,
                               int src_hi) = 0;
    virtual void OpTlsCmp(ThreadOffset offset, int val) = 0;
    virtual bool InexpensiveConstantInt(int32_t value) = 0;
    virtual bool InexpensiveConstantFloat(int32_t value) = 0;
    virtual bool InexpensiveConstantLong(int64_t value) = 0;
    virtual bool InexpensiveConstantDouble(int64_t value) = 0;

    // Temp workaround
    void Workaround7250540(RegLocation rl_dest, int value);

  protected:
    Mir2Lir(CompilationUnit* cu, MIRGraph* mir_graph, ArenaAllocator* arena);

    CompilationUnit* GetCompilationUnit() {
      return cu_;
    }

  private:
    void GenInstanceofFinal(bool use_declaring_class, uint32_t type_idx, RegLocation rl_dest,
                            RegLocation rl_src);
    void GenInstanceofCallingHelper(bool needs_access_check, bool type_known_final,
                                    bool type_known_abstract, bool use_declaring_class,
                                    bool can_assume_type_is_in_dex_cache,
                                    uint32_t type_idx, RegLocation rl_dest,
                                    RegLocation rl_src);

    void ClobberBody(RegisterInfo* p);
    void ResetDefBody(RegisterInfo* p) {
      p->def_start = NULL;
      p->def_end = NULL;
    }

  public:
    // TODO: add accessors for these.
    LIR* literal_list_;                        // Constants.
    LIR* method_literal_list_;                 // Method literals requiring patching.
    LIR* code_literal_list_;                   // Code literals requiring patching.

  protected:
    CompilationUnit* const cu_;
    MIRGraph* const mir_graph_;
    GrowableArray<SwitchTable*> switch_tables_;
    GrowableArray<FillArrayData*> fill_array_data_;
    GrowableArray<LIR*> throw_launchpads_;
    GrowableArray<LIR*> suspend_launchpads_;
    GrowableArray<LIR*> intrinsic_launchpads_;
    SafeMap<unsigned int, LIR*> boundary_map_;  // boundary lookup cache.
    /*
     * Holds mapping from native PC to dex PC for safepoints where we may deoptimize.
     * Native PC is on the return address of the safepointed operation.  Dex PC is for
     * the instruction being executed at the safepoint.
     */
    std::vector<uint32_t> pc2dex_mapping_table_;
    /*
     * Holds mapping from Dex PC to native PC for catch entry points.  Native PC and Dex PC
     * immediately preceed the instruction.
     */
    std::vector<uint32_t> dex2pc_mapping_table_;
    int data_offset_;                     // starting offset of literal pool.
    int total_size_;                      // header + code size.
    LIR* block_label_list_;
    PromotionMap* promotion_map_;
    /*
     * TODO: The code generation utilities don't have a built-in
     * mechanism to propagate the original Dalvik opcode address to the
     * associated generated instructions.  For the trace compiler, this wasn't
     * necessary because the interpreter handled all throws and debugging
     * requests.  For now we'll handle this by placing the Dalvik offset
     * in the CompilationUnit struct before codegen for each instruction.
     * The low-level LIR creation utilites will pull it from here.  Rework this.
     */
    int current_dalvik_offset_;
    RegisterPool* reg_pool_;
    /*
     * Sanity checking for the register temp tracking.  The same ssa
     * name should never be associated with one temp register per
     * instruction compilation.
     */
    int live_sreg_;
    CodeBuffer code_buffer_;
    // The encoding mapping table data (dex -> pc offset and pc offset -> dex) with a size prefix.
    UnsignedLeb128EncodingVector encoded_mapping_table_;
    std::vector<uint32_t> core_vmap_table_;
    std::vector<uint32_t> fp_vmap_table_;
    std::vector<uint8_t> native_gc_map_;
    int num_core_spills_;
    int num_fp_spills_;
    int frame_size_;
    unsigned int core_spill_mask_;
    unsigned int fp_spill_mask_;
    LIR* first_lir_insn_;
    LIR* last_lir_insn_;
};  // Class Mir2Lir

}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_MIR_TO_LIR_H_
