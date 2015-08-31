/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_COMPILER_DEX_PORTABLE_MIR_TO_GBC_H_
#define ART_COMPILER_DEX_PORTABLE_MIR_TO_GBC_H_

#include "invoke_type.h"
#include "compiled_method.h"
#include "dex/compiler_enums.h"
#include "dex/compiler_ir.h"
#include "dex/backend.h"
#include "llvm/llvm_compilation_unit.h"
#include "safe_map.h"

namespace art {

struct BasicBlock;
struct CallInfo;
struct CompilationUnit;
struct MIR;
struct RegLocation;
struct RegisterInfo;
class MIRGraph;

// Target-specific initialization.
Backend* PortableCodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                               ArenaAllocator* const arena,
                               llvm::LlvmCompilationUnit* const llvm_compilation_unit);

class MirConverter : public Backend {
  public:
    // TODO: flesh out and integrate into new world order.
    MirConverter(CompilationUnit* cu, MIRGraph* mir_graph, ArenaAllocator* arena,
                 llvm::LlvmCompilationUnit* llvm_compilation_unit)
      : Backend(arena),
        cu_(cu),
        mir_graph_(mir_graph),
        llvm_compilation_unit_(llvm_compilation_unit),
        llvm_info_(llvm_compilation_unit->GetQuickContext()),
        symbol_(llvm_compilation_unit->GetDexCompilationUnit()->GetSymbol()),
        context_(NULL),
        module_(NULL),
        func_(NULL),
        intrinsic_helper_(NULL),
        irb_(NULL),
        placeholder_bb_(NULL),
        entry_bb_(NULL),
        entry_target_bb_(NULL),
        llvm_values_(arena, mir_graph->GetNumSSARegs()),
        temp_name_(0),
        current_dalvik_offset_(0) {
      if (kIsDebugBuild) {
        cu->enable_debug |= (1 << kDebugVerifyBitcode);
      }
    }

    void Materialize() {
      MethodMIR2Bitcode();
    }

    CompiledMethod* GetCompiledMethod() {
      return NULL;
    }

  private:
    ::llvm::BasicBlock* GetLLVMBlock(int id);
    ::llvm::Value* GetLLVMValue(int s_reg);
    void SetVregOnValue(::llvm::Value* val, int s_reg);
    void DefineValueOnly(::llvm::Value* val, int s_reg);
    void DefineValue(::llvm::Value* val, int s_reg);
    ::llvm::Type* LlvmTypeFromLocRec(RegLocation loc);
    void InitIR();
    ::llvm::BasicBlock* FindCaseTarget(uint32_t vaddr);
    void ConvertPackedSwitch(BasicBlock* bb, int32_t table_offset,
                             RegLocation rl_src);
    void ConvertSparseSwitch(BasicBlock* bb, int32_t table_offset,
                             RegLocation rl_src);
    void ConvertSget(int32_t field_index,
                     art::llvm::IntrinsicHelper::IntrinsicId id, RegLocation rl_dest);
    void ConvertSput(int32_t field_index,
                     art::llvm::IntrinsicHelper::IntrinsicId id, RegLocation rl_src);
    void ConvertFillArrayData(int32_t offset, RegLocation rl_array);
    ::llvm::Value* EmitConst(::llvm::ArrayRef< ::llvm::Value*> src,
                             RegLocation loc);
    void EmitPopShadowFrame();
    ::llvm::Value* EmitCopy(::llvm::ArrayRef< ::llvm::Value*> src,
                            RegLocation loc);
    void ConvertMoveException(RegLocation rl_dest);
    void ConvertThrow(RegLocation rl_src);
    void ConvertMonitorEnterExit(int opt_flags,
                                 art::llvm::IntrinsicHelper::IntrinsicId id, RegLocation rl_src);
    void ConvertArrayLength(int opt_flags, RegLocation rl_dest,
                            RegLocation rl_src);
    void EmitSuspendCheck();
    ::llvm::Value* ConvertCompare(ConditionCode cc,
                                  ::llvm::Value* src1, ::llvm::Value* src2);
    void ConvertCompareAndBranch(BasicBlock* bb, MIR* mir, ConditionCode cc,
                                 RegLocation rl_src1, RegLocation rl_src2);
    void ConvertCompareZeroAndBranch(BasicBlock* bb, MIR* mir, ConditionCode cc,
                                     RegLocation rl_src1);
    ::llvm::Value* GenDivModOp(bool is_div, bool is_long, ::llvm::Value* src1,
                               ::llvm::Value* src2);
    ::llvm::Value* GenArithOp(OpKind op, bool is_long, ::llvm::Value* src1,
                              ::llvm::Value* src2);
    void ConvertFPArithOp(OpKind op, RegLocation rl_dest, RegLocation rl_src1,
                          RegLocation rl_src2);
    void ConvertShift(art::llvm::IntrinsicHelper::IntrinsicId id,
                      RegLocation rl_dest, RegLocation rl_src1, RegLocation rl_src2);
    void ConvertShiftLit(art::llvm::IntrinsicHelper::IntrinsicId id,
                         RegLocation rl_dest, RegLocation rl_src, int shift_amount);
    void ConvertArithOp(OpKind op, RegLocation rl_dest, RegLocation rl_src1,
                        RegLocation rl_src2);
    void ConvertArithOpLit(OpKind op, RegLocation rl_dest, RegLocation rl_src1,
                           int32_t imm);
    void ConvertInvoke(BasicBlock* bb, MIR* mir, InvokeType invoke_type,
                       bool is_range, bool is_filled_new_array);
    void ConvertConstObject(uint32_t idx,
                            art::llvm::IntrinsicHelper::IntrinsicId id, RegLocation rl_dest);
    void ConvertCheckCast(uint32_t type_idx, RegLocation rl_src);
    void ConvertNewInstance(uint32_t type_idx, RegLocation rl_dest);
    void ConvertNewArray(uint32_t type_idx, RegLocation rl_dest,
                         RegLocation rl_src);
    void ConvertAget(int opt_flags, art::llvm::IntrinsicHelper::IntrinsicId id,
                     RegLocation rl_dest, RegLocation rl_array, RegLocation rl_index);
    void ConvertAput(int opt_flags, art::llvm::IntrinsicHelper::IntrinsicId id,
                     RegLocation rl_src, RegLocation rl_array, RegLocation rl_index);
    void ConvertIget(int opt_flags, art::llvm::IntrinsicHelper::IntrinsicId id,
                     RegLocation rl_dest, RegLocation rl_obj, int field_index);
    void ConvertIput(int opt_flags, art::llvm::IntrinsicHelper::IntrinsicId id,
                     RegLocation rl_src, RegLocation rl_obj, int field_index);
    void ConvertInstanceOf(uint32_t type_idx, RegLocation rl_dest,
                           RegLocation rl_src);
    void ConvertIntToLong(RegLocation rl_dest, RegLocation rl_src);
    void ConvertLongToInt(RegLocation rl_dest, RegLocation rl_src);
    void ConvertFloatToDouble(RegLocation rl_dest, RegLocation rl_src);
    void ConvertDoubleToFloat(RegLocation rl_dest, RegLocation rl_src);
    void ConvertWideComparison(art::llvm::IntrinsicHelper::IntrinsicId id,
                               RegLocation rl_dest, RegLocation rl_src1, RegLocation rl_src2);
    void ConvertIntNarrowing(RegLocation rl_dest, RegLocation rl_src,
                             art::llvm::IntrinsicHelper::IntrinsicId id);
    void ConvertNeg(RegLocation rl_dest, RegLocation rl_src);
    void ConvertIntToFP(::llvm::Type* ty, RegLocation rl_dest, RegLocation rl_src);
    void ConvertFPToInt(art::llvm::IntrinsicHelper::IntrinsicId id,
                        RegLocation rl_dest, RegLocation rl_src);
    void ConvertNegFP(RegLocation rl_dest, RegLocation rl_src);
    void ConvertNot(RegLocation rl_dest, RegLocation rl_src);
    void EmitConstructorBarrier();
    bool ConvertMIRNode(MIR* mir, BasicBlock* bb, ::llvm::BasicBlock* llvm_bb);
    void SetDexOffset(int32_t offset);
    void SetMethodInfo();
    void HandlePhiNodes(BasicBlock* bb, ::llvm::BasicBlock* llvm_bb);
    void ConvertExtendedMIR(BasicBlock* bb, MIR* mir, ::llvm::BasicBlock* llvm_bb);
    bool BlockBitcodeConversion(BasicBlock* bb);
    ::llvm::FunctionType* GetFunctionType();
    bool CreateFunction();
    bool CreateLLVMBasicBlock(BasicBlock* bb);
    void MethodMIR2Bitcode();

    CompilationUnit* cu_;
    MIRGraph* mir_graph_;
    llvm::LlvmCompilationUnit* const llvm_compilation_unit_;
    LLVMInfo* llvm_info_;
    std::string symbol_;
    ::llvm::LLVMContext* context_;
    ::llvm::Module* module_;
    ::llvm::Function* func_;
    art::llvm::IntrinsicHelper* intrinsic_helper_;
    art::llvm::IRBuilder* irb_;
    ::llvm::BasicBlock* placeholder_bb_;
    ::llvm::BasicBlock* entry_bb_;
    ::llvm::BasicBlock* entry_target_bb_;
    std::string bitcode_filename_;
    GrowableArray< ::llvm::Value*> llvm_values_;
    int32_t temp_name_;
    SafeMap<int32_t, ::llvm::BasicBlock*> id_to_block_map_;  // block id -> llvm bb.
    int current_dalvik_offset_;
};  // Class MirConverter

}  // namespace art

#endif  // ART_COMPILER_DEX_PORTABLE_MIR_TO_GBC_H_
