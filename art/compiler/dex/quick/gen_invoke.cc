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

#include "dex/compiler_ir.h"
#include "dex_file-inl.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "invoke_type.h"
#include "mirror/array.h"
#include "mirror/string.h"
#include "mir_to_lir-inl.h"
#include "x86/codegen_x86.h"

namespace art {

/*
 * This source files contains "gen" codegen routines that should
 * be applicable to most targets.  Only mid-level support utilities
 * and "op" calls may be used here.
 */

/*
 * To save scheduling time, helper calls are broken into two parts: generation of
 * the helper target address, and the actual call to the helper.  Because x86
 * has a memory call operation, part 1 is a NOP for x86.  For other targets,
 * load arguments between the two parts.
 */
int Mir2Lir::CallHelperSetup(ThreadOffset helper_offset) {
  return (cu_->instruction_set == kX86) ? 0 : LoadHelper(helper_offset);
}

/* NOTE: if r_tgt is a temp, it will be freed following use */
LIR* Mir2Lir::CallHelper(int r_tgt, ThreadOffset helper_offset, bool safepoint_pc, bool use_link) {
  LIR* call_inst;
  OpKind op = use_link ? kOpBlx : kOpBx;
  if (cu_->instruction_set == kX86) {
    call_inst = OpThreadMem(op, helper_offset);
  } else {
    call_inst = OpReg(op, r_tgt);
    FreeTemp(r_tgt);
  }
  if (safepoint_pc) {
    MarkSafepointPC(call_inst);
  }
  return call_inst;
}

void Mir2Lir::CallRuntimeHelperImm(ThreadOffset helper_offset, int arg0, bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  LoadConstant(TargetReg(kArg0), arg0);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperReg(ThreadOffset helper_offset, int arg0, bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  OpRegCopy(TargetReg(kArg0), arg0);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperRegLocation(ThreadOffset helper_offset, RegLocation arg0,
                                           bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  if (arg0.wide == 0) {
    LoadValueDirectFixed(arg0, TargetReg(kArg0));
  } else {
    LoadValueDirectWideFixed(arg0, TargetReg(kArg0), TargetReg(kArg1));
  }
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperImmImm(ThreadOffset helper_offset, int arg0, int arg1,
                                      bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  LoadConstant(TargetReg(kArg0), arg0);
  LoadConstant(TargetReg(kArg1), arg1);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperImmRegLocation(ThreadOffset helper_offset, int arg0,
                                              RegLocation arg1, bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  if (arg1.wide == 0) {
    LoadValueDirectFixed(arg1, TargetReg(kArg1));
  } else {
    LoadValueDirectWideFixed(arg1, TargetReg(kArg1), TargetReg(kArg2));
  }
  LoadConstant(TargetReg(kArg0), arg0);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperRegLocationImm(ThreadOffset helper_offset, RegLocation arg0, int arg1,
                                              bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  LoadValueDirectFixed(arg0, TargetReg(kArg0));
  LoadConstant(TargetReg(kArg1), arg1);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperImmReg(ThreadOffset helper_offset, int arg0, int arg1,
                                      bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  OpRegCopy(TargetReg(kArg1), arg1);
  LoadConstant(TargetReg(kArg0), arg0);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperRegImm(ThreadOffset helper_offset, int arg0, int arg1,
                                      bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  OpRegCopy(TargetReg(kArg0), arg0);
  LoadConstant(TargetReg(kArg1), arg1);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperImmMethod(ThreadOffset helper_offset, int arg0, bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  LoadCurrMethodDirect(TargetReg(kArg1));
  LoadConstant(TargetReg(kArg0), arg0);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperRegLocationRegLocation(ThreadOffset helper_offset, RegLocation arg0,
                                                      RegLocation arg1, bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  if (arg0.wide == 0) {
    LoadValueDirectFixed(arg0, arg0.fp ? TargetReg(kFArg0) : TargetReg(kArg0));
    if (arg1.wide == 0) {
      if (cu_->instruction_set == kMips) {
        LoadValueDirectFixed(arg1, arg1.fp ? TargetReg(kFArg2) : TargetReg(kArg1));
      } else {
        LoadValueDirectFixed(arg1, TargetReg(kArg1));
      }
    } else {
      if (cu_->instruction_set == kMips) {
        LoadValueDirectWideFixed(arg1, arg1.fp ? TargetReg(kFArg2) : TargetReg(kArg1), arg1.fp ? TargetReg(kFArg3) : TargetReg(kArg2));
      } else {
        LoadValueDirectWideFixed(arg1, TargetReg(kArg1), TargetReg(kArg2));
      }
    }
  } else {
    LoadValueDirectWideFixed(arg0, arg0.fp ? TargetReg(kFArg0) : TargetReg(kArg0), arg0.fp ? TargetReg(kFArg1) : TargetReg(kArg1));
    if (arg1.wide == 0) {
      LoadValueDirectFixed(arg1, arg1.fp ? TargetReg(kFArg2) : TargetReg(kArg2));
    } else {
      LoadValueDirectWideFixed(arg1, arg1.fp ? TargetReg(kFArg2) : TargetReg(kArg2), arg1.fp ? TargetReg(kFArg3) : TargetReg(kArg3));
    }
  }
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperRegReg(ThreadOffset helper_offset, int arg0, int arg1,
                                      bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  DCHECK_NE(TargetReg(kArg0), arg1);  // check copy into arg0 won't clobber arg1
  OpRegCopy(TargetReg(kArg0), arg0);
  OpRegCopy(TargetReg(kArg1), arg1);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperRegRegImm(ThreadOffset helper_offset, int arg0, int arg1,
                                         int arg2, bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  DCHECK_NE(TargetReg(kArg0), arg1);  // check copy into arg0 won't clobber arg1
  OpRegCopy(TargetReg(kArg0), arg0);
  OpRegCopy(TargetReg(kArg1), arg1);
  LoadConstant(TargetReg(kArg2), arg2);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperImmMethodRegLocation(ThreadOffset helper_offset,
                                                    int arg0, RegLocation arg2, bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  LoadValueDirectFixed(arg2, TargetReg(kArg2));
  LoadCurrMethodDirect(TargetReg(kArg1));
  LoadConstant(TargetReg(kArg0), arg0);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperImmMethodImm(ThreadOffset helper_offset, int arg0,
                                            int arg2, bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  LoadCurrMethodDirect(TargetReg(kArg1));
  LoadConstant(TargetReg(kArg2), arg2);
  LoadConstant(TargetReg(kArg0), arg0);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

void Mir2Lir::CallRuntimeHelperImmRegLocationRegLocation(ThreadOffset helper_offset,
                                                         int arg0, RegLocation arg1,
                                                         RegLocation arg2, bool safepoint_pc) {
  int r_tgt = CallHelperSetup(helper_offset);
  LoadValueDirectFixed(arg1, TargetReg(kArg1));
  if (arg2.wide == 0) {
    LoadValueDirectFixed(arg2, TargetReg(kArg2));
  } else {
    LoadValueDirectWideFixed(arg2, TargetReg(kArg2), TargetReg(kArg3));
  }
  LoadConstant(TargetReg(kArg0), arg0);
  ClobberCalleeSave();
  CallHelper(r_tgt, helper_offset, safepoint_pc);
}

/*
 * If there are any ins passed in registers that have not been promoted
 * to a callee-save register, flush them to the frame.  Perform intial
 * assignment of promoted arguments.
 *
 * ArgLocs is an array of location records describing the incoming arguments
 * with one location record per word of argument.
 */
void Mir2Lir::FlushIns(RegLocation* ArgLocs, RegLocation rl_method) {
  /*
   * Dummy up a RegLocation for the incoming Method*
   * It will attempt to keep kArg0 live (or copy it to home location
   * if promoted).
   */
  RegLocation rl_src = rl_method;
  rl_src.location = kLocPhysReg;
  rl_src.low_reg = TargetReg(kArg0);
  rl_src.home = false;
  MarkLive(rl_src.low_reg, rl_src.s_reg_low);
  StoreValue(rl_method, rl_src);
  // If Method* has been promoted, explicitly flush
  if (rl_method.location == kLocPhysReg) {
    StoreWordDisp(TargetReg(kSp), 0, TargetReg(kArg0));
  }

  if (cu_->num_ins == 0)
    return;
  const int num_arg_regs = 3;
  static SpecialTargetRegister arg_regs[] = {kArg1, kArg2, kArg3};
  int start_vreg = cu_->num_dalvik_registers - cu_->num_ins;
  /*
   * Copy incoming arguments to their proper home locations.
   * NOTE: an older version of dx had an issue in which
   * it would reuse static method argument registers.
   * This could result in the same Dalvik virtual register
   * being promoted to both core and fp regs. To account for this,
   * we only copy to the corresponding promoted physical register
   * if it matches the type of the SSA name for the incoming
   * argument.  It is also possible that long and double arguments
   * end up half-promoted.  In those cases, we must flush the promoted
   * half to memory as well.
   */
  for (int i = 0; i < cu_->num_ins; i++) {
    PromotionMap* v_map = &promotion_map_[start_vreg + i];
    if (i < num_arg_regs) {
      // If arriving in register
      bool need_flush = true;
      RegLocation* t_loc = &ArgLocs[i];
      if ((v_map->core_location == kLocPhysReg) && !t_loc->fp) {
        OpRegCopy(v_map->core_reg, TargetReg(arg_regs[i]));
        need_flush = false;
      } else if ((v_map->fp_location == kLocPhysReg) && t_loc->fp) {
        OpRegCopy(v_map->FpReg, TargetReg(arg_regs[i]));
        need_flush = false;
      } else {
        need_flush = true;
      }

      // For wide args, force flush if not fully promoted
      if (t_loc->wide) {
        PromotionMap* p_map = v_map + (t_loc->high_word ? -1 : +1);
        // Is only half promoted?
        need_flush |= (p_map->core_location != v_map->core_location) ||
            (p_map->fp_location != v_map->fp_location);
        if ((cu_->instruction_set == kThumb2) && t_loc->fp && !need_flush) {
          /*
           * In Arm, a double is represented as a pair of consecutive single float
           * registers starting at an even number.  It's possible that both Dalvik vRegs
           * representing the incoming double were independently promoted as singles - but
           * not in a form usable as a double.  If so, we need to flush - even though the
           * incoming arg appears fully in register.  At this point in the code, both
           * halves of the double are promoted.  Make sure they are in a usable form.
           */
          int lowreg_index = start_vreg + i + (t_loc->high_word ? -1 : 0);
          int low_reg = promotion_map_[lowreg_index].FpReg;
          int high_reg = promotion_map_[lowreg_index + 1].FpReg;
          if (((low_reg & 0x1) != 0) || (high_reg != (low_reg + 1))) {
            need_flush = true;
          }
        }
      }
      if (need_flush) {
        StoreBaseDisp(TargetReg(kSp), SRegOffset(start_vreg + i),
                      TargetReg(arg_regs[i]), kWord);
      }
    } else {
      // If arriving in frame & promoted
      if (v_map->core_location == kLocPhysReg) {
        LoadWordDisp(TargetReg(kSp), SRegOffset(start_vreg + i),
                     v_map->core_reg);
      }
      if (v_map->fp_location == kLocPhysReg) {
        LoadWordDisp(TargetReg(kSp), SRegOffset(start_vreg + i),
                     v_map->FpReg);
      }
    }
  }
}

/*
 * Bit of a hack here - in the absence of a real scheduling pass,
 * emit the next instruction in static & direct invoke sequences.
 */
static int NextSDCallInsn(CompilationUnit* cu, CallInfo* info,
                          int state, const MethodReference& target_method,
                          uint32_t unused,
                          uintptr_t direct_code, uintptr_t direct_method,
                          InvokeType type) {
  Mir2Lir* cg = static_cast<Mir2Lir*>(cu->cg.get());
  if (cu->instruction_set != kThumb2) {
    // Disable sharpening
    direct_code = 0;
    direct_method = 0;
  }
  if (direct_code != 0 && direct_method != 0) {
    switch (state) {
    case 0:  // Get the current Method* [sets kArg0]
      if (direct_code != static_cast<unsigned int>(-1)) {
        cg->LoadConstant(cg->TargetReg(kInvokeTgt), direct_code);
      } else {
        CHECK_EQ(cu->dex_file, target_method.dex_file);
        LIR* data_target = cg->ScanLiteralPool(cg->code_literal_list_,
                                               target_method.dex_method_index, 0);
        if (data_target == NULL) {
          data_target = cg->AddWordData(&cg->code_literal_list_, target_method.dex_method_index);
          data_target->operands[1] = type;
        }
        LIR* load_pc_rel = cg->OpPcRelLoad(cg->TargetReg(kInvokeTgt), data_target);
        cg->AppendLIR(load_pc_rel);
        DCHECK_EQ(cu->instruction_set, kThumb2) << reinterpret_cast<void*>(data_target);
      }
      if (direct_method != static_cast<unsigned int>(-1)) {
        cg->LoadConstant(cg->TargetReg(kArg0), direct_method);
      } else {
        CHECK_EQ(cu->dex_file, target_method.dex_file);
        LIR* data_target = cg->ScanLiteralPool(cg->method_literal_list_,
                                               target_method.dex_method_index, 0);
        if (data_target == NULL) {
          data_target = cg->AddWordData(&cg->method_literal_list_, target_method.dex_method_index);
          data_target->operands[1] = type;
        }
        LIR* load_pc_rel = cg->OpPcRelLoad(cg->TargetReg(kArg0), data_target);
        cg->AppendLIR(load_pc_rel);
        DCHECK_EQ(cu->instruction_set, kThumb2) << reinterpret_cast<void*>(data_target);
      }
      break;
    default:
      return -1;
    }
  } else {
    switch (state) {
    case 0:  // Get the current Method* [sets kArg0]
      // TUNING: we can save a reg copy if Method* has been promoted.
      cg->LoadCurrMethodDirect(cg->TargetReg(kArg0));
      break;
    case 1:  // Get method->dex_cache_resolved_methods_
      cg->LoadWordDisp(cg->TargetReg(kArg0),
        mirror::ArtMethod::DexCacheResolvedMethodsOffset().Int32Value(), cg->TargetReg(kArg0));
      // Set up direct code if known.
      if (direct_code != 0) {
        if (direct_code != static_cast<unsigned int>(-1)) {
          cg->LoadConstant(cg->TargetReg(kInvokeTgt), direct_code);
        } else {
          CHECK_EQ(cu->dex_file, target_method.dex_file);
          LIR* data_target = cg->ScanLiteralPool(cg->code_literal_list_,
                                                 target_method.dex_method_index, 0);
          if (data_target == NULL) {
            data_target = cg->AddWordData(&cg->code_literal_list_, target_method.dex_method_index);
            data_target->operands[1] = type;
          }
          LIR* load_pc_rel = cg->OpPcRelLoad(cg->TargetReg(kInvokeTgt), data_target);
          cg->AppendLIR(load_pc_rel);
          DCHECK_EQ(cu->instruction_set, kThumb2) << reinterpret_cast<void*>(data_target);
        }
      }
      break;
    case 2:  // Grab target method*
      CHECK_EQ(cu->dex_file, target_method.dex_file);
      cg->LoadWordDisp(cg->TargetReg(kArg0),
                       mirror::Array::DataOffset(sizeof(mirror::Object*)).Int32Value() +
                           (target_method.dex_method_index * 4),
                       cg-> TargetReg(kArg0));
      break;
    case 3:  // Grab the code from the method*
      if (cu->instruction_set != kX86) {
        if (direct_code == 0) {
          cg->LoadWordDisp(cg->TargetReg(kArg0),
                           mirror::ArtMethod::GetEntryPointFromCompiledCodeOffset().Int32Value(),
                           cg->TargetReg(kInvokeTgt));
        }
        break;
      }
      // Intentional fallthrough for x86
    default:
      return -1;
    }
  }
  return state + 1;
}

/*
 * Bit of a hack here - in the absence of a real scheduling pass,
 * emit the next instruction in a virtual invoke sequence.
 * We can use kLr as a temp prior to target address loading
 * Note also that we'll load the first argument ("this") into
 * kArg1 here rather than the standard LoadArgRegs.
 */
static int NextVCallInsn(CompilationUnit* cu, CallInfo* info,
                         int state, const MethodReference& target_method,
                         uint32_t method_idx, uintptr_t unused, uintptr_t unused2,
                         InvokeType unused3) {
  Mir2Lir* cg = static_cast<Mir2Lir*>(cu->cg.get());
  /*
   * This is the fast path in which the target virtual method is
   * fully resolved at compile time.
   */
  switch (state) {
    case 0: {  // Get "this" [set kArg1]
      RegLocation  rl_arg = info->args[0];
      cg->LoadValueDirectFixed(rl_arg, cg->TargetReg(kArg1));
      break;
    }
    case 1:  // Is "this" null? [use kArg1]
      cg->GenNullCheck(info->args[0].s_reg_low, cg->TargetReg(kArg1), info->opt_flags);
      // get this->klass_ [use kArg1, set kInvokeTgt]
      cg->LoadWordDisp(cg->TargetReg(kArg1), mirror::Object::ClassOffset().Int32Value(),
                       cg->TargetReg(kInvokeTgt));
      break;
    case 2:  // Get this->klass_->vtable [usr kInvokeTgt, set kInvokeTgt]
      cg->LoadWordDisp(cg->TargetReg(kInvokeTgt), mirror::Class::VTableOffset().Int32Value(),
                       cg->TargetReg(kInvokeTgt));
      break;
    case 3:  // Get target method [use kInvokeTgt, set kArg0]
      cg->LoadWordDisp(cg->TargetReg(kInvokeTgt), (method_idx * 4) +
                       mirror::Array::DataOffset(sizeof(mirror::Object*)).Int32Value(),
                       cg->TargetReg(kArg0));
      break;
    case 4:  // Get the compiled code address [uses kArg0, sets kInvokeTgt]
      if (cu->instruction_set != kX86) {
        cg->LoadWordDisp(cg->TargetReg(kArg0),
                         mirror::ArtMethod::GetEntryPointFromCompiledCodeOffset().Int32Value(),
                         cg->TargetReg(kInvokeTgt));
        break;
      }
      // Intentional fallthrough for X86
    default:
      return -1;
  }
  return state + 1;
}

/*
 * All invoke-interface calls bounce off of art_quick_invoke_interface_trampoline,
 * which will locate the target and continue on via a tail call.
 */
static int NextInterfaceCallInsn(CompilationUnit* cu, CallInfo* info, int state,
                                 const MethodReference& target_method,
                                 uint32_t unused, uintptr_t unused2,
                                 uintptr_t direct_method, InvokeType unused4) {
  Mir2Lir* cg = static_cast<Mir2Lir*>(cu->cg.get());
  if (cu->instruction_set != kThumb2) {
    // Disable sharpening
    direct_method = 0;
  }
  ThreadOffset trampoline = QUICK_ENTRYPOINT_OFFSET(pInvokeInterfaceTrampoline);

  if (direct_method != 0) {
    switch (state) {
      case 0:  // Load the trampoline target [sets kInvokeTgt].
        if (cu->instruction_set != kX86) {
          cg->LoadWordDisp(cg->TargetReg(kSelf), trampoline.Int32Value(),
                           cg->TargetReg(kInvokeTgt));
        }
        // Get the interface Method* [sets kArg0]
        if (direct_method != static_cast<unsigned int>(-1)) {
          cg->LoadConstant(cg->TargetReg(kArg0), direct_method);
        } else {
          CHECK_EQ(cu->dex_file, target_method.dex_file);
          LIR* data_target = cg->ScanLiteralPool(cg->method_literal_list_,
                                                 target_method.dex_method_index, 0);
          if (data_target == NULL) {
            data_target = cg->AddWordData(&cg->method_literal_list_,
                                          target_method.dex_method_index);
            data_target->operands[1] = kInterface;
          }
          LIR* load_pc_rel = cg->OpPcRelLoad(cg->TargetReg(kArg0), data_target);
          cg->AppendLIR(load_pc_rel);
          DCHECK_EQ(cu->instruction_set, kThumb2) << reinterpret_cast<void*>(data_target);
        }
        break;
      default:
        return -1;
    }
  } else {
    switch (state) {
      case 0:
        // Get the current Method* [sets kArg0] - TUNING: remove copy of method if it is promoted.
        cg->LoadCurrMethodDirect(cg->TargetReg(kArg0));
        // Load the trampoline target [sets kInvokeTgt].
        if (cu->instruction_set != kX86) {
          cg->LoadWordDisp(cg->TargetReg(kSelf), trampoline.Int32Value(),
                           cg->TargetReg(kInvokeTgt));
        }
        break;
    case 1:  // Get method->dex_cache_resolved_methods_ [set/use kArg0]
      cg->LoadWordDisp(cg->TargetReg(kArg0),
                       mirror::ArtMethod::DexCacheResolvedMethodsOffset().Int32Value(),
                       cg->TargetReg(kArg0));
      break;
    case 2:  // Grab target method* [set/use kArg0]
      CHECK_EQ(cu->dex_file, target_method.dex_file);
      cg->LoadWordDisp(cg->TargetReg(kArg0),
                       mirror::Array::DataOffset(sizeof(mirror::Object*)).Int32Value() +
                           (target_method.dex_method_index * 4),
                       cg->TargetReg(kArg0));
      break;
    default:
      return -1;
    }
  }
  return state + 1;
}

static int NextInvokeInsnSP(CompilationUnit* cu, CallInfo* info, ThreadOffset trampoline,
                            int state, const MethodReference& target_method,
                            uint32_t method_idx) {
  Mir2Lir* cg = static_cast<Mir2Lir*>(cu->cg.get());
  /*
   * This handles the case in which the base method is not fully
   * resolved at compile time, we bail to a runtime helper.
   */
  if (state == 0) {
    if (cu->instruction_set != kX86) {
      // Load trampoline target
      cg->LoadWordDisp(cg->TargetReg(kSelf), trampoline.Int32Value(), cg->TargetReg(kInvokeTgt));
    }
    // Load kArg0 with method index
    CHECK_EQ(cu->dex_file, target_method.dex_file);
    cg->LoadConstant(cg->TargetReg(kArg0), target_method.dex_method_index);
    return 1;
  }
  return -1;
}

static int NextStaticCallInsnSP(CompilationUnit* cu, CallInfo* info,
                                int state,
                                const MethodReference& target_method,
                                uint32_t method_idx,
                                uintptr_t unused, uintptr_t unused2,
                                InvokeType unused3) {
  ThreadOffset trampoline = QUICK_ENTRYPOINT_OFFSET(pInvokeStaticTrampolineWithAccessCheck);
  return NextInvokeInsnSP(cu, info, trampoline, state, target_method, 0);
}

static int NextDirectCallInsnSP(CompilationUnit* cu, CallInfo* info, int state,
                                const MethodReference& target_method,
                                uint32_t method_idx, uintptr_t unused,
                                uintptr_t unused2, InvokeType unused3) {
  ThreadOffset trampoline = QUICK_ENTRYPOINT_OFFSET(pInvokeDirectTrampolineWithAccessCheck);
  return NextInvokeInsnSP(cu, info, trampoline, state, target_method, 0);
}

static int NextSuperCallInsnSP(CompilationUnit* cu, CallInfo* info, int state,
                               const MethodReference& target_method,
                               uint32_t method_idx, uintptr_t unused,
                               uintptr_t unused2, InvokeType unused3) {
  ThreadOffset trampoline = QUICK_ENTRYPOINT_OFFSET(pInvokeSuperTrampolineWithAccessCheck);
  return NextInvokeInsnSP(cu, info, trampoline, state, target_method, 0);
}

static int NextVCallInsnSP(CompilationUnit* cu, CallInfo* info, int state,
                           const MethodReference& target_method,
                           uint32_t method_idx, uintptr_t unused,
                           uintptr_t unused2, InvokeType unused3) {
  ThreadOffset trampoline = QUICK_ENTRYPOINT_OFFSET(pInvokeVirtualTrampolineWithAccessCheck);
  return NextInvokeInsnSP(cu, info, trampoline, state, target_method, 0);
}

static int NextInterfaceCallInsnWithAccessCheck(CompilationUnit* cu,
                                                CallInfo* info, int state,
                                                const MethodReference& target_method,
                                                uint32_t unused,
                                                uintptr_t unused2, uintptr_t unused3,
                                                InvokeType unused4) {
  ThreadOffset trampoline = QUICK_ENTRYPOINT_OFFSET(pInvokeInterfaceTrampolineWithAccessCheck);
  return NextInvokeInsnSP(cu, info, trampoline, state, target_method, 0);
}

int Mir2Lir::LoadArgRegs(CallInfo* info, int call_state,
                         NextCallInsn next_call_insn,
                         const MethodReference& target_method,
                         uint32_t vtable_idx, uintptr_t direct_code,
                         uintptr_t direct_method, InvokeType type, bool skip_this) {
  int last_arg_reg = TargetReg(kArg3);
  int next_reg = TargetReg(kArg1);
  int next_arg = 0;
  if (skip_this) {
    next_reg++;
    next_arg++;
  }
  for (; (next_reg <= last_arg_reg) && (next_arg < info->num_arg_words); next_reg++) {
    RegLocation rl_arg = info->args[next_arg++];
    rl_arg = UpdateRawLoc(rl_arg);
    if (rl_arg.wide && (next_reg <= TargetReg(kArg2))) {
      LoadValueDirectWideFixed(rl_arg, next_reg, next_reg + 1);
      next_reg++;
      next_arg++;
    } else {
      if (rl_arg.wide) {
        rl_arg.wide = false;
        rl_arg.is_const = false;
      }
      LoadValueDirectFixed(rl_arg, next_reg);
    }
    call_state = next_call_insn(cu_, info, call_state, target_method, vtable_idx,
                                direct_code, direct_method, type);
  }
  return call_state;
}

/*
 * Load up to 5 arguments, the first three of which will be in
 * kArg1 .. kArg3.  On entry kArg0 contains the current method pointer,
 * and as part of the load sequence, it must be replaced with
 * the target method pointer.  Note, this may also be called
 * for "range" variants if the number of arguments is 5 or fewer.
 */
int Mir2Lir::GenDalvikArgsNoRange(CallInfo* info,
                                  int call_state, LIR** pcrLabel, NextCallInsn next_call_insn,
                                  const MethodReference& target_method,
                                  uint32_t vtable_idx, uintptr_t direct_code,
                                  uintptr_t direct_method, InvokeType type, bool skip_this) {
  RegLocation rl_arg;

  /* If no arguments, just return */
  if (info->num_arg_words == 0)
    return call_state;

  call_state = next_call_insn(cu_, info, call_state, target_method, vtable_idx,
                              direct_code, direct_method, type);

  DCHECK_LE(info->num_arg_words, 5);
  if (info->num_arg_words > 3) {
    int32_t next_use = 3;
    // Detect special case of wide arg spanning arg3/arg4
    RegLocation rl_use0 = info->args[0];
    RegLocation rl_use1 = info->args[1];
    RegLocation rl_use2 = info->args[2];
    if (((!rl_use0.wide && !rl_use1.wide) || rl_use0.wide) &&
      rl_use2.wide) {
      int reg = -1;
      // Wide spans, we need the 2nd half of uses[2].
      rl_arg = UpdateLocWide(rl_use2);
      if (rl_arg.location == kLocPhysReg) {
        reg = rl_arg.high_reg;
      } else {
        // kArg2 & rArg3 can safely be used here
        reg = TargetReg(kArg3);
        LoadWordDisp(TargetReg(kSp), SRegOffset(rl_arg.s_reg_low) + 4, reg);
        call_state = next_call_insn(cu_, info, call_state, target_method,
                                    vtable_idx, direct_code, direct_method, type);
      }
      StoreBaseDisp(TargetReg(kSp), (next_use + 1) * 4, reg, kWord);
      StoreBaseDisp(TargetReg(kSp), 16 /* (3+1)*4 */, reg, kWord);
      call_state = next_call_insn(cu_, info, call_state, target_method, vtable_idx,
                                  direct_code, direct_method, type);
      next_use++;
    }
    // Loop through the rest
    while (next_use < info->num_arg_words) {
      int low_reg;
      int high_reg = -1;
      rl_arg = info->args[next_use];
      rl_arg = UpdateRawLoc(rl_arg);
      if (rl_arg.location == kLocPhysReg) {
        low_reg = rl_arg.low_reg;
        high_reg = rl_arg.high_reg;
      } else {
        low_reg = TargetReg(kArg2);
        if (rl_arg.wide) {
          high_reg = TargetReg(kArg3);
          LoadValueDirectWideFixed(rl_arg, low_reg, high_reg);
        } else {
          LoadValueDirectFixed(rl_arg, low_reg);
        }
        call_state = next_call_insn(cu_, info, call_state, target_method,
                                    vtable_idx, direct_code, direct_method, type);
      }
      int outs_offset = (next_use + 1) * 4;
      if (rl_arg.wide) {
        StoreBaseDispWide(TargetReg(kSp), outs_offset, low_reg, high_reg);
        next_use += 2;
      } else {
        StoreWordDisp(TargetReg(kSp), outs_offset, low_reg);
        next_use++;
      }
      call_state = next_call_insn(cu_, info, call_state, target_method, vtable_idx,
                               direct_code, direct_method, type);
    }
  }

  call_state = LoadArgRegs(info, call_state, next_call_insn,
                           target_method, vtable_idx, direct_code, direct_method,
                           type, skip_this);

  if (pcrLabel) {
    *pcrLabel = GenNullCheck(info->args[0].s_reg_low, TargetReg(kArg1), info->opt_flags);
  }
  return call_state;
}

/*
 * May have 0+ arguments (also used for jumbo).  Note that
 * source virtual registers may be in physical registers, so may
 * need to be flushed to home location before copying.  This
 * applies to arg3 and above (see below).
 *
 * Two general strategies:
 *    If < 20 arguments
 *       Pass args 3-18 using vldm/vstm block copy
 *       Pass arg0, arg1 & arg2 in kArg1-kArg3
 *    If 20+ arguments
 *       Pass args arg19+ using memcpy block copy
 *       Pass arg0, arg1 & arg2 in kArg1-kArg3
 *
 */
int Mir2Lir::GenDalvikArgsRange(CallInfo* info, int call_state,
                                LIR** pcrLabel, NextCallInsn next_call_insn,
                                const MethodReference& target_method,
                                uint32_t vtable_idx, uintptr_t direct_code, uintptr_t direct_method,
                                InvokeType type, bool skip_this) {
  // If we can treat it as non-range (Jumbo ops will use range form)
  if (info->num_arg_words <= 5)
    return GenDalvikArgsNoRange(info, call_state, pcrLabel,
                                next_call_insn, target_method, vtable_idx,
                                direct_code, direct_method, type, skip_this);
  /*
   * First load the non-register arguments.  Both forms expect all
   * of the source arguments to be in their home frame location, so
   * scan the s_reg names and flush any that have been promoted to
   * frame backing storage.
   */
  // Scan the rest of the args - if in phys_reg flush to memory
  for (int next_arg = 0; next_arg < info->num_arg_words;) {
    RegLocation loc = info->args[next_arg];
    if (loc.wide) {
      loc = UpdateLocWide(loc);
      if ((next_arg >= 2) && (loc.location == kLocPhysReg)) {
        StoreBaseDispWide(TargetReg(kSp), SRegOffset(loc.s_reg_low),
                          loc.low_reg, loc.high_reg);
      }
      next_arg += 2;
    } else {
      loc = UpdateLoc(loc);
      if ((next_arg >= 3) && (loc.location == kLocPhysReg)) {
        StoreBaseDisp(TargetReg(kSp), SRegOffset(loc.s_reg_low),
                      loc.low_reg, kWord);
      }
      next_arg++;
    }
  }

  int start_offset = SRegOffset(info->args[3].s_reg_low);
  int outs_offset = 4 /* Method* */ + (3 * 4);
  if (cu_->instruction_set != kThumb2) {
    // Generate memcpy
    OpRegRegImm(kOpAdd, TargetReg(kArg0), TargetReg(kSp), outs_offset);
    OpRegRegImm(kOpAdd, TargetReg(kArg1), TargetReg(kSp), start_offset);
    CallRuntimeHelperRegRegImm(QUICK_ENTRYPOINT_OFFSET(pMemcpy), TargetReg(kArg0),
                               TargetReg(kArg1), (info->num_arg_words - 3) * 4, false);
  } else {
    if (info->num_arg_words >= 20) {
      // Generate memcpy
      OpRegRegImm(kOpAdd, TargetReg(kArg0), TargetReg(kSp), outs_offset);
      OpRegRegImm(kOpAdd, TargetReg(kArg1), TargetReg(kSp), start_offset);
      CallRuntimeHelperRegRegImm(QUICK_ENTRYPOINT_OFFSET(pMemcpy), TargetReg(kArg0),
                                 TargetReg(kArg1), (info->num_arg_words - 3) * 4, false);
    } else {
      // Use vldm/vstm pair using kArg3 as a temp
      int regs_left = std::min(info->num_arg_words - 3, 16);
      call_state = next_call_insn(cu_, info, call_state, target_method, vtable_idx,
                               direct_code, direct_method, type);
      OpRegRegImm(kOpAdd, TargetReg(kArg3), TargetReg(kSp), start_offset);
      LIR* ld = OpVldm(TargetReg(kArg3), regs_left);
      // TUNING: loosen barrier
      ld->def_mask = ENCODE_ALL;
      SetMemRefType(ld, true /* is_load */, kDalvikReg);
      call_state = next_call_insn(cu_, info, call_state, target_method, vtable_idx,
                               direct_code, direct_method, type);
      OpRegRegImm(kOpAdd, TargetReg(kArg3), TargetReg(kSp), 4 /* Method* */ + (3 * 4));
      call_state = next_call_insn(cu_, info, call_state, target_method, vtable_idx,
                               direct_code, direct_method, type);
      LIR* st = OpVstm(TargetReg(kArg3), regs_left);
      SetMemRefType(st, false /* is_load */, kDalvikReg);
      st->def_mask = ENCODE_ALL;
      call_state = next_call_insn(cu_, info, call_state, target_method, vtable_idx,
                               direct_code, direct_method, type);
    }
  }

  call_state = LoadArgRegs(info, call_state, next_call_insn,
                           target_method, vtable_idx, direct_code, direct_method,
                           type, skip_this);

  call_state = next_call_insn(cu_, info, call_state, target_method, vtable_idx,
                           direct_code, direct_method, type);
  if (pcrLabel) {
    *pcrLabel = GenNullCheck(info->args[0].s_reg_low, TargetReg(kArg1), info->opt_flags);
  }
  return call_state;
}

RegLocation Mir2Lir::InlineTarget(CallInfo* info) {
  RegLocation res;
  if (info->result.location == kLocInvalid) {
    res = GetReturn(false);
  } else {
    res = info->result;
  }
  return res;
}

RegLocation Mir2Lir::InlineTargetWide(CallInfo* info) {
  RegLocation res;
  if (info->result.location == kLocInvalid) {
    res = GetReturnWide(false);
  } else {
    res = info->result;
  }
  return res;
}

bool Mir2Lir::GenInlinedCharAt(CallInfo* info) {
  if (cu_->instruction_set == kMips) {
    // TODO - add Mips implementation
    return false;
  }
  // Location of reference to data array
  int value_offset = mirror::String::ValueOffset().Int32Value();
  // Location of count
  int count_offset = mirror::String::CountOffset().Int32Value();
  // Starting offset within data array
  int offset_offset = mirror::String::OffsetOffset().Int32Value();
  // Start of char data with array_
  int data_offset = mirror::Array::DataOffset(sizeof(uint16_t)).Int32Value();

  RegLocation rl_obj = info->args[0];
  RegLocation rl_idx = info->args[1];
  rl_obj = LoadValue(rl_obj, kCoreReg);
  rl_idx = LoadValue(rl_idx, kCoreReg);
  int reg_max;
  GenNullCheck(rl_obj.s_reg_low, rl_obj.low_reg, info->opt_flags);
  bool range_check = (!(info->opt_flags & MIR_IGNORE_RANGE_CHECK));
  LIR* launch_pad = NULL;
  int reg_off = INVALID_REG;
  int reg_ptr = INVALID_REG;
  if (cu_->instruction_set != kX86) {
    reg_off = AllocTemp();
    reg_ptr = AllocTemp();
    if (range_check) {
      reg_max = AllocTemp();
      LoadWordDisp(rl_obj.low_reg, count_offset, reg_max);
    }
    LoadWordDisp(rl_obj.low_reg, offset_offset, reg_off);
    LoadWordDisp(rl_obj.low_reg, value_offset, reg_ptr);
    if (range_check) {
      // Set up a launch pad to allow retry in case of bounds violation */
      launch_pad = RawLIR(0, kPseudoIntrinsicRetry, reinterpret_cast<uintptr_t>(info));
      intrinsic_launchpads_.Insert(launch_pad);
      OpRegReg(kOpCmp, rl_idx.low_reg, reg_max);
      FreeTemp(reg_max);
      OpCondBranch(kCondCs, launch_pad);
    }
  } else {
    if (range_check) {
      reg_max = AllocTemp();
      LoadWordDisp(rl_obj.low_reg, count_offset, reg_max);
      // Set up a launch pad to allow retry in case of bounds violation */
      launch_pad = RawLIR(0, kPseudoIntrinsicRetry, reinterpret_cast<uintptr_t>(info));
      intrinsic_launchpads_.Insert(launch_pad);
      OpRegReg(kOpCmp, rl_idx.low_reg, reg_max);
      FreeTemp(reg_max);
      OpCondBranch(kCondCc, launch_pad);
    }
    reg_off = AllocTemp();
    reg_ptr = AllocTemp();
    LoadWordDisp(rl_obj.low_reg, offset_offset, reg_off);
    LoadWordDisp(rl_obj.low_reg, value_offset, reg_ptr);
  }
  OpRegImm(kOpAdd, reg_ptr, data_offset);
  OpRegReg(kOpAdd, reg_off, rl_idx.low_reg);
  FreeTemp(rl_obj.low_reg);
  FreeTemp(rl_idx.low_reg);
  RegLocation rl_dest = InlineTarget(info);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  LoadBaseIndexed(reg_ptr, reg_off, rl_result.low_reg, 1, kUnsignedHalf);
  FreeTemp(reg_off);
  FreeTemp(reg_ptr);
  StoreValue(rl_dest, rl_result);
  if (range_check) {
    launch_pad->operands[2] = 0;  // no resumption
  }
  // Record that we've already inlined & null checked
  info->opt_flags |= (MIR_INLINED | MIR_IGNORE_NULL_CHECK);
  return true;
}

// Generates an inlined String.is_empty or String.length.
bool Mir2Lir::GenInlinedStringIsEmptyOrLength(CallInfo* info, bool is_empty) {
  if (cu_->instruction_set == kMips) {
    // TODO - add Mips implementation
    return false;
  }
  // dst = src.length();
  RegLocation rl_obj = info->args[0];
  rl_obj = LoadValue(rl_obj, kCoreReg);
  RegLocation rl_dest = InlineTarget(info);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  GenNullCheck(rl_obj.s_reg_low, rl_obj.low_reg, info->opt_flags);
  LoadWordDisp(rl_obj.low_reg, mirror::String::CountOffset().Int32Value(), rl_result.low_reg);
  if (is_empty) {
    // dst = (dst == 0);
    if (cu_->instruction_set == kThumb2) {
      int t_reg = AllocTemp();
      OpRegReg(kOpNeg, t_reg, rl_result.low_reg);
      OpRegRegReg(kOpAdc, rl_result.low_reg, rl_result.low_reg, t_reg);
    } else {
      DCHECK_EQ(cu_->instruction_set, kX86);
      OpRegImm(kOpSub, rl_result.low_reg, 1);
      OpRegImm(kOpLsr, rl_result.low_reg, 31);
    }
  }
  StoreValue(rl_dest, rl_result);
  return true;
}

bool Mir2Lir::GenInlinedAbsInt(CallInfo* info) {
  if (cu_->instruction_set == kMips) {
    // TODO - add Mips implementation
    return false;
  }
  RegLocation rl_src = info->args[0];
  rl_src = LoadValue(rl_src, kCoreReg);
  RegLocation rl_dest = InlineTarget(info);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  int sign_reg = AllocTemp();
  // abs(x) = y<=x>>31, (x+y)^y.
  OpRegRegImm(kOpAsr, sign_reg, rl_src.low_reg, 31);
  OpRegRegReg(kOpAdd, rl_result.low_reg, rl_src.low_reg, sign_reg);
  OpRegReg(kOpXor, rl_result.low_reg, sign_reg);
  StoreValue(rl_dest, rl_result);
  return true;
}

bool Mir2Lir::GenInlinedAbsLong(CallInfo* info) {
  if (cu_->instruction_set == kMips) {
    // TODO - add Mips implementation
    return false;
  }
  if (cu_->instruction_set == kThumb2) {
    RegLocation rl_src = info->args[0];
    rl_src = LoadValueWide(rl_src, kCoreReg);
    RegLocation rl_dest = InlineTargetWide(info);
    RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
    int sign_reg = AllocTemp();
    // abs(x) = y<=x>>31, (x+y)^y.
    OpRegRegImm(kOpAsr, sign_reg, rl_src.high_reg, 31);
    OpRegRegReg(kOpAdd, rl_result.low_reg, rl_src.low_reg, sign_reg);
    OpRegRegReg(kOpAdc, rl_result.high_reg, rl_src.high_reg, sign_reg);
    OpRegReg(kOpXor, rl_result.low_reg, sign_reg);
    OpRegReg(kOpXor, rl_result.high_reg, sign_reg);
    StoreValueWide(rl_dest, rl_result);
    return true;
  } else {
    DCHECK_EQ(cu_->instruction_set, kX86);
    // Reuse source registers to avoid running out of temps
    RegLocation rl_src = info->args[0];
    rl_src = LoadValueWide(rl_src, kCoreReg);
    RegLocation rl_dest = InlineTargetWide(info);
    RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
    OpRegCopyWide(rl_result.low_reg, rl_result.high_reg, rl_src.low_reg, rl_src.high_reg);
    FreeTemp(rl_src.low_reg);
    FreeTemp(rl_src.high_reg);
    int sign_reg = AllocTemp();
    // abs(x) = y<=x>>31, (x+y)^y.
    OpRegRegImm(kOpAsr, sign_reg, rl_result.high_reg, 31);
    OpRegReg(kOpAdd, rl_result.low_reg, sign_reg);
    OpRegReg(kOpAdc, rl_result.high_reg, sign_reg);
    OpRegReg(kOpXor, rl_result.low_reg, sign_reg);
    OpRegReg(kOpXor, rl_result.high_reg, sign_reg);
    StoreValueWide(rl_dest, rl_result);
    return true;
  }
}

bool Mir2Lir::GenInlinedFloatCvt(CallInfo* info) {
  if (cu_->instruction_set == kMips) {
    // TODO - add Mips implementation
    return false;
  }
  RegLocation rl_src = info->args[0];
  RegLocation rl_dest = InlineTarget(info);
  StoreValue(rl_dest, rl_src);
  return true;
}

bool Mir2Lir::GenInlinedDoubleCvt(CallInfo* info) {
  if (cu_->instruction_set == kMips) {
    // TODO - add Mips implementation
    return false;
  }
  RegLocation rl_src = info->args[0];
  RegLocation rl_dest = InlineTargetWide(info);
  StoreValueWide(rl_dest, rl_src);
  return true;
}

/*
 * Fast string.index_of(I) & (II).  Tests for simple case of char <= 0xffff,
 * otherwise bails to standard library code.
 */
bool Mir2Lir::GenInlinedIndexOf(CallInfo* info, bool zero_based) {
  if (cu_->instruction_set == kMips) {
    // TODO - add Mips implementation
    return false;
  }
  ClobberCalleeSave();
  LockCallTemps();  // Using fixed registers
  int reg_ptr = TargetReg(kArg0);
  int reg_char = TargetReg(kArg1);
  int reg_start = TargetReg(kArg2);

  RegLocation rl_obj = info->args[0];
  RegLocation rl_char = info->args[1];
  RegLocation rl_start = info->args[2];
  LoadValueDirectFixed(rl_obj, reg_ptr);
  LoadValueDirectFixed(rl_char, reg_char);
  if (zero_based) {
    LoadConstant(reg_start, 0);
  } else {
    LoadValueDirectFixed(rl_start, reg_start);
  }
  int r_tgt = (cu_->instruction_set != kX86) ? LoadHelper(QUICK_ENTRYPOINT_OFFSET(pIndexOf)) : 0;
  GenNullCheck(rl_obj.s_reg_low, reg_ptr, info->opt_flags);
  LIR* launch_pad = RawLIR(0, kPseudoIntrinsicRetry, reinterpret_cast<uintptr_t>(info));
  intrinsic_launchpads_.Insert(launch_pad);
  OpCmpImmBranch(kCondGt, reg_char, 0xFFFF, launch_pad);
  // NOTE: not a safepoint
  if (cu_->instruction_set != kX86) {
    OpReg(kOpBlx, r_tgt);
  } else {
    OpThreadMem(kOpBlx, QUICK_ENTRYPOINT_OFFSET(pIndexOf));
  }
  LIR* resume_tgt = NewLIR0(kPseudoTargetLabel);
  launch_pad->operands[2] = reinterpret_cast<uintptr_t>(resume_tgt);
  // Record that we've already inlined & null checked
  info->opt_flags |= (MIR_INLINED | MIR_IGNORE_NULL_CHECK);
  RegLocation rl_return = GetReturn(false);
  RegLocation rl_dest = InlineTarget(info);
  StoreValue(rl_dest, rl_return);
  return true;
}

/* Fast string.compareTo(Ljava/lang/string;)I. */
bool Mir2Lir::GenInlinedStringCompareTo(CallInfo* info) {
  if (cu_->instruction_set == kMips) {
    // TODO - add Mips implementation
    return false;
  }
  ClobberCalleeSave();
  LockCallTemps();  // Using fixed registers
  int reg_this = TargetReg(kArg0);
  int reg_cmp = TargetReg(kArg1);

  RegLocation rl_this = info->args[0];
  RegLocation rl_cmp = info->args[1];
  LoadValueDirectFixed(rl_this, reg_this);
  LoadValueDirectFixed(rl_cmp, reg_cmp);
  int r_tgt = (cu_->instruction_set != kX86) ?
      LoadHelper(QUICK_ENTRYPOINT_OFFSET(pStringCompareTo)) : 0;
  GenNullCheck(rl_this.s_reg_low, reg_this, info->opt_flags);
  // TUNING: check if rl_cmp.s_reg_low is already null checked
  LIR* launch_pad = RawLIR(0, kPseudoIntrinsicRetry, reinterpret_cast<uintptr_t>(info));
  intrinsic_launchpads_.Insert(launch_pad);
  OpCmpImmBranch(kCondEq, reg_cmp, 0, launch_pad);
  // NOTE: not a safepoint
  if (cu_->instruction_set != kX86) {
    OpReg(kOpBlx, r_tgt);
  } else {
    OpThreadMem(kOpBlx, QUICK_ENTRYPOINT_OFFSET(pStringCompareTo));
  }
  launch_pad->operands[2] = 0;  // No return possible
  // Record that we've already inlined & null checked
  info->opt_flags |= (MIR_INLINED | MIR_IGNORE_NULL_CHECK);
  RegLocation rl_return = GetReturn(false);
  RegLocation rl_dest = InlineTarget(info);
  StoreValue(rl_dest, rl_return);
  return true;
}

bool Mir2Lir::GenInlinedCurrentThread(CallInfo* info) {
  RegLocation rl_dest = InlineTarget(info);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  ThreadOffset offset = Thread::PeerOffset();
  if (cu_->instruction_set == kThumb2 || cu_->instruction_set == kMips) {
    LoadWordDisp(TargetReg(kSelf), offset.Int32Value(), rl_result.low_reg);
  } else {
    CHECK(cu_->instruction_set == kX86);
    reinterpret_cast<X86Mir2Lir*>(this)->OpRegThreadMem(kOpMov, rl_result.low_reg, offset);
  }
  StoreValue(rl_dest, rl_result);
  return true;
}

bool Mir2Lir::GenInlinedUnsafeGet(CallInfo* info,
                                  bool is_long, bool is_volatile) {
  if (cu_->instruction_set == kMips) {
    // TODO - add Mips implementation
    return false;
  }
  // Unused - RegLocation rl_src_unsafe = info->args[0];
  RegLocation rl_src_obj = info->args[1];  // Object
  RegLocation rl_src_offset = info->args[2];  // long low
  rl_src_offset.wide = 0;  // ignore high half in info->args[3]
  RegLocation rl_dest = InlineTarget(info);  // result reg
  if (is_volatile) {
    GenMemBarrier(kLoadLoad);
  }
  RegLocation rl_object = LoadValue(rl_src_obj, kCoreReg);
  RegLocation rl_offset = LoadValue(rl_src_offset, kCoreReg);
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  if (is_long) {
    OpRegReg(kOpAdd, rl_object.low_reg, rl_offset.low_reg);
    LoadBaseDispWide(rl_object.low_reg, 0, rl_result.low_reg, rl_result.high_reg, INVALID_SREG);
    StoreValueWide(rl_dest, rl_result);
  } else {
    LoadBaseIndexed(rl_object.low_reg, rl_offset.low_reg, rl_result.low_reg, 0, kWord);
    StoreValue(rl_dest, rl_result);
  }
  return true;
}

bool Mir2Lir::GenInlinedUnsafePut(CallInfo* info, bool is_long,
                                  bool is_object, bool is_volatile, bool is_ordered) {
  if (cu_->instruction_set == kMips) {
    // TODO - add Mips implementation
    return false;
  }
  if (cu_->instruction_set == kX86 && is_object) {
    // TODO: fix X86, it exhausts registers for card marking.
    return false;
  }
  // Unused - RegLocation rl_src_unsafe = info->args[0];
  RegLocation rl_src_obj = info->args[1];  // Object
  RegLocation rl_src_offset = info->args[2];  // long low
  rl_src_offset.wide = 0;  // ignore high half in info->args[3]
  RegLocation rl_src_value = info->args[4];  // value to store
  if (is_volatile || is_ordered) {
    GenMemBarrier(kStoreStore);
  }
  RegLocation rl_object = LoadValue(rl_src_obj, kCoreReg);
  RegLocation rl_offset = LoadValue(rl_src_offset, kCoreReg);
  RegLocation rl_value;
  if (is_long) {
    rl_value = LoadValueWide(rl_src_value, kCoreReg);
    OpRegReg(kOpAdd, rl_object.low_reg, rl_offset.low_reg);
    StoreBaseDispWide(rl_object.low_reg, 0, rl_value.low_reg, rl_value.high_reg);
  } else {
    rl_value = LoadValue(rl_src_value, kCoreReg);
    StoreBaseIndexed(rl_object.low_reg, rl_offset.low_reg, rl_value.low_reg, 0, kWord);
  }
  if (is_volatile) {
    GenMemBarrier(kStoreLoad);
  }
  if (is_object) {
    MarkGCCard(rl_value.low_reg, rl_object.low_reg);
  }
  return true;
}

bool Mir2Lir::GenIntrinsic(CallInfo* info) {
  if (info->opt_flags & MIR_INLINED) {
    return false;
  }
  /*
   * TODO: move these to a target-specific structured constant array
   * and use a generic match function.  The list of intrinsics may be
   * slightly different depending on target.
   * TODO: Fold this into a matching function that runs during
   * basic block building.  This should be part of the action for
   * small method inlining and recognition of the special object init
   * method.  By doing this during basic block construction, we can also
   * take advantage of/generate new useful dataflow info.
   */
  StringPiece tgt_methods_declaring_class(
      cu_->dex_file->GetMethodDeclaringClassDescriptor(cu_->dex_file->GetMethodId(info->index)));
  if (tgt_methods_declaring_class.starts_with("Ljava/lang/Double;")) {
    std::string tgt_method(PrettyMethod(info->index, *cu_->dex_file));
    if (tgt_method == "long java.lang.Double.doubleToRawLongBits(double)") {
      return GenInlinedDoubleCvt(info);
    }
    if (tgt_method == "double java.lang.Double.longBitsToDouble(long)") {
      return GenInlinedDoubleCvt(info);
    }
  } else if (tgt_methods_declaring_class.starts_with("Ljava/lang/Float;")) {
    std::string tgt_method(PrettyMethod(info->index, *cu_->dex_file));
    if (tgt_method == "int java.lang.Float.float_to_raw_int_bits(float)") {
      return GenInlinedFloatCvt(info);
    }
    if (tgt_method == "float java.lang.Float.intBitsToFloat(int)") {
      return GenInlinedFloatCvt(info);
    }
  } else if (tgt_methods_declaring_class.starts_with("Ljava/lang/Math;") ||
             tgt_methods_declaring_class.starts_with("Ljava/lang/StrictMath;")) {
    std::string tgt_method(PrettyMethod(info->index, *cu_->dex_file));
    if (tgt_method == "int java.lang.Math.abs(int)" ||
        tgt_method == "int java.lang.StrictMath.abs(int)") {
      return GenInlinedAbsInt(info);
    }
    if (tgt_method == "long java.lang.Math.abs(long)" ||
        tgt_method == "long java.lang.StrictMath.abs(long)") {
      return GenInlinedAbsLong(info);
    }
    if (tgt_method == "int java.lang.Math.max(int, int)" ||
        tgt_method == "int java.lang.StrictMath.max(int, int)") {
      return GenInlinedMinMaxInt(info, false /* is_min */);
    }
    if (tgt_method == "int java.lang.Math.min(int, int)" ||
        tgt_method == "int java.lang.StrictMath.min(int, int)") {
      return GenInlinedMinMaxInt(info, true /* is_min */);
    }
    if (tgt_method == "double java.lang.Math.sqrt(double)" ||
        tgt_method == "double java.lang.StrictMath.sqrt(double)") {
      return GenInlinedSqrt(info);
    }
  } else if (tgt_methods_declaring_class.starts_with("Ljava/lang/String;")) {
    std::string tgt_method(PrettyMethod(info->index, *cu_->dex_file));
    if (tgt_method == "char java.lang.String.charAt(int)") {
      return GenInlinedCharAt(info);
    }
    if (tgt_method == "int java.lang.String.compareTo(java.lang.String)") {
      return GenInlinedStringCompareTo(info);
    }
    if (tgt_method == "boolean java.lang.String.is_empty()") {
      return GenInlinedStringIsEmptyOrLength(info, true /* is_empty */);
    }
    if (tgt_method == "int java.lang.String.index_of(int, int)") {
      return GenInlinedIndexOf(info, false /* base 0 */);
    }
    if (tgt_method == "int java.lang.String.index_of(int)") {
      return GenInlinedIndexOf(info, true /* base 0 */);
    }
    if (tgt_method == "int java.lang.String.length()") {
      return GenInlinedStringIsEmptyOrLength(info, false /* is_empty */);
    }
  } else if (tgt_methods_declaring_class.starts_with("Ljava/lang/Thread;")) {
    std::string tgt_method(PrettyMethod(info->index, *cu_->dex_file));
    if (tgt_method == "java.lang.Thread java.lang.Thread.currentThread()") {
      return GenInlinedCurrentThread(info);
    }
  } else if (tgt_methods_declaring_class.starts_with("Lsun/misc/Unsafe;")) {
    std::string tgt_method(PrettyMethod(info->index, *cu_->dex_file));
    if (tgt_method == "boolean sun.misc.Unsafe.compareAndSwapInt(java.lang.Object, long, int, int)") {
      return GenInlinedCas32(info, false);
    }
    if (tgt_method == "boolean sun.misc.Unsafe.compareAndSwapObject(java.lang.Object, long, java.lang.Object, java.lang.Object)") {
      return GenInlinedCas32(info, true);
    }
    if (tgt_method == "int sun.misc.Unsafe.getInt(java.lang.Object, long)") {
      return GenInlinedUnsafeGet(info, false /* is_long */, false /* is_volatile */);
    }
    if (tgt_method == "int sun.misc.Unsafe.getIntVolatile(java.lang.Object, long)") {
      return GenInlinedUnsafeGet(info, false /* is_long */, true /* is_volatile */);
    }
    if (tgt_method == "void sun.misc.Unsafe.putInt(java.lang.Object, long, int)") {
      return GenInlinedUnsafePut(info, false /* is_long */, false /* is_object */,
                                 false /* is_volatile */, false /* is_ordered */);
    }
    if (tgt_method == "void sun.misc.Unsafe.putIntVolatile(java.lang.Object, long, int)") {
      return GenInlinedUnsafePut(info, false /* is_long */, false /* is_object */,
                                 true /* is_volatile */, false /* is_ordered */);
    }
    if (tgt_method == "void sun.misc.Unsafe.putOrderedInt(java.lang.Object, long, int)") {
      return GenInlinedUnsafePut(info, false /* is_long */, false /* is_object */,
                                 false /* is_volatile */, true /* is_ordered */);
    }
    if (tgt_method == "long sun.misc.Unsafe.getLong(java.lang.Object, long)") {
      return GenInlinedUnsafeGet(info, true /* is_long */, false /* is_volatile */);
    }
    if (tgt_method == "long sun.misc.Unsafe.getLongVolatile(java.lang.Object, long)") {
      return GenInlinedUnsafeGet(info, true /* is_long */, true /* is_volatile */);
    }
    if (tgt_method == "void sun.misc.Unsafe.putLong(java.lang.Object, long, long)") {
      return GenInlinedUnsafePut(info, true /* is_long */, false /* is_object */,
                                 false /* is_volatile */, false /* is_ordered */);
    }
    if (tgt_method == "void sun.misc.Unsafe.putLongVolatile(java.lang.Object, long, long)") {
      return GenInlinedUnsafePut(info, true /* is_long */, false /* is_object */,
                                 true /* is_volatile */, false /* is_ordered */);
    }
    if (tgt_method == "void sun.misc.Unsafe.putOrderedLong(java.lang.Object, long, long)") {
      return GenInlinedUnsafePut(info, true /* is_long */, false /* is_object */,
                                 false /* is_volatile */, true /* is_ordered */);
    }
    if (tgt_method == "java.lang.Object sun.misc.Unsafe.getObject(java.lang.Object, long)") {
      return GenInlinedUnsafeGet(info, false /* is_long */, false /* is_volatile */);
    }
    if (tgt_method == "java.lang.Object sun.misc.Unsafe.getObjectVolatile(java.lang.Object, long)") {
      return GenInlinedUnsafeGet(info, false /* is_long */, true /* is_volatile */);
    }
    if (tgt_method == "void sun.misc.Unsafe.putObject(java.lang.Object, long, java.lang.Object)") {
      return GenInlinedUnsafePut(info, false /* is_long */, true /* is_object */,
                                 false /* is_volatile */, false /* is_ordered */);
    }
    if (tgt_method == "void sun.misc.Unsafe.putObjectVolatile(java.lang.Object, long, java.lang.Object)") {
      return GenInlinedUnsafePut(info, false /* is_long */, true /* is_object */,
                                 true /* is_volatile */, false /* is_ordered */);
    }
    if (tgt_method == "void sun.misc.Unsafe.putOrderedObject(java.lang.Object, long, java.lang.Object)") {
      return GenInlinedUnsafePut(info, false /* is_long */, true /* is_object */,
                                 false /* is_volatile */, true /* is_ordered */);
    }
  }
  return false;
}

void Mir2Lir::GenInvoke(CallInfo* info) {
  if (GenIntrinsic(info)) {
    return;
  }
  InvokeType original_type = info->type;  // avoiding mutation by ComputeInvokeInfo
  int call_state = 0;
  LIR* null_ck;
  LIR** p_null_ck = NULL;
  NextCallInsn next_call_insn;
  FlushAllRegs();  /* Everything to home location */
  // Explicit register usage
  LockCallTemps();

  DexCompilationUnit* cUnit = mir_graph_->GetCurrentDexCompilationUnit();
  MethodReference target_method(cUnit->GetDexFile(), info->index);
  int vtable_idx;
  uintptr_t direct_code;
  uintptr_t direct_method;
  bool skip_this;
  bool fast_path =
      cu_->compiler_driver->ComputeInvokeInfo(mir_graph_->GetCurrentDexCompilationUnit(),
                                              current_dalvik_offset_,
                                              info->type, target_method,
                                              vtable_idx,
                                              direct_code, direct_method,
                                              true) && !SLOW_INVOKE_PATH;
  if (info->type == kInterface) {
    if (fast_path) {
      p_null_ck = &null_ck;
    }
    next_call_insn = fast_path ? NextInterfaceCallInsn : NextInterfaceCallInsnWithAccessCheck;
    skip_this = false;
  } else if (info->type == kDirect) {
    if (fast_path) {
      p_null_ck = &null_ck;
    }
    next_call_insn = fast_path ? NextSDCallInsn : NextDirectCallInsnSP;
    skip_this = false;
  } else if (info->type == kStatic) {
    next_call_insn = fast_path ? NextSDCallInsn : NextStaticCallInsnSP;
    skip_this = false;
  } else if (info->type == kSuper) {
    DCHECK(!fast_path);  // Fast path is a direct call.
    next_call_insn = NextSuperCallInsnSP;
    skip_this = false;
  } else {
    DCHECK_EQ(info->type, kVirtual);
    next_call_insn = fast_path ? NextVCallInsn : NextVCallInsnSP;
    skip_this = fast_path;
  }
  if (!info->is_range) {
    call_state = GenDalvikArgsNoRange(info, call_state, p_null_ck,
                                      next_call_insn, target_method,
                                      vtable_idx, direct_code, direct_method,
                                      original_type, skip_this);
  } else {
    call_state = GenDalvikArgsRange(info, call_state, p_null_ck,
                                    next_call_insn, target_method, vtable_idx,
                                    direct_code, direct_method, original_type,
                                    skip_this);
  }
  // Finish up any of the call sequence not interleaved in arg loading
  while (call_state >= 0) {
    call_state = next_call_insn(cu_, info, call_state, target_method,
                                vtable_idx, direct_code, direct_method,
                                original_type);
  }
  LIR* call_inst;
  if (cu_->instruction_set != kX86) {
    call_inst = OpReg(kOpBlx, TargetReg(kInvokeTgt));
  } else {
    if (fast_path && info->type != kInterface) {
      call_inst = OpMem(kOpBlx, TargetReg(kArg0),
                        mirror::ArtMethod::GetEntryPointFromCompiledCodeOffset().Int32Value());
    } else {
      ThreadOffset trampoline(-1);
      switch (info->type) {
      case kInterface:
        trampoline = fast_path ? QUICK_ENTRYPOINT_OFFSET(pInvokeInterfaceTrampoline)
            : QUICK_ENTRYPOINT_OFFSET(pInvokeInterfaceTrampolineWithAccessCheck);
        break;
      case kDirect:
        trampoline = QUICK_ENTRYPOINT_OFFSET(pInvokeDirectTrampolineWithAccessCheck);
        break;
      case kStatic:
        trampoline = QUICK_ENTRYPOINT_OFFSET(pInvokeStaticTrampolineWithAccessCheck);
        break;
      case kSuper:
        trampoline = QUICK_ENTRYPOINT_OFFSET(pInvokeSuperTrampolineWithAccessCheck);
        break;
      case kVirtual:
        trampoline = QUICK_ENTRYPOINT_OFFSET(pInvokeVirtualTrampolineWithAccessCheck);
        break;
      default:
        LOG(FATAL) << "Unexpected invoke type";
      }
      call_inst = OpThreadMem(kOpBlx, trampoline);
    }
  }
  MarkSafepointPC(call_inst);

  ClobberCalleeSave();
  if (info->result.location != kLocInvalid) {
    // We have a following MOVE_RESULT - do it now.
    if (info->result.wide) {
      RegLocation ret_loc = GetReturnWide(info->result.fp);
      StoreValueWide(info->result, ret_loc);
    } else {
      RegLocation ret_loc = GetReturn(info->result.fp);
      StoreValue(info->result, ret_loc);
    }
  }
}

}  // namespace art
