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

#include "codegen_x86.h"
#include "dex/compiler_internals.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "x86_lir.h"

#include <string>

namespace art {

// FIXME: restore "static" when usage uncovered
/*static*/ int core_regs[] = {
  rAX, rCX, rDX, rBX, rX86_SP, rBP, rSI, rDI
#ifdef TARGET_REX_SUPPORT
  r8, r9, r10, r11, r12, r13, r14, 15
#endif
};
/*static*/ int ReservedRegs[] = {rX86_SP};
/*static*/ int core_temps[] = {rAX, rCX, rDX, rBX};
/*static*/ int FpRegs[] = {
  fr0, fr1, fr2, fr3, fr4, fr5, fr6, fr7,
#ifdef TARGET_REX_SUPPORT
  fr8, fr9, fr10, fr11, fr12, fr13, fr14, fr15
#endif
};
/*static*/ int fp_temps[] = {
  fr0, fr1, fr2, fr3, fr4, fr5, fr6, fr7,
#ifdef TARGET_REX_SUPPORT
  fr8, fr9, fr10, fr11, fr12, fr13, fr14, fr15
#endif
};

RegLocation X86Mir2Lir::LocCReturn() {
  RegLocation res = X86_LOC_C_RETURN;
  return res;
}

RegLocation X86Mir2Lir::LocCReturnWide() {
  RegLocation res = X86_LOC_C_RETURN_WIDE;
  return res;
}

RegLocation X86Mir2Lir::LocCReturnFloat() {
  RegLocation res = X86_LOC_C_RETURN_FLOAT;
  return res;
}

RegLocation X86Mir2Lir::LocCReturnDouble() {
  RegLocation res = X86_LOC_C_RETURN_DOUBLE;
  return res;
}

// Return a target-dependent special register.
int X86Mir2Lir::TargetReg(SpecialTargetRegister reg) {
  int res = INVALID_REG;
  switch (reg) {
    case kSelf: res = rX86_SELF; break;
    case kSuspend: res =  rX86_SUSPEND; break;
    case kLr: res =  rX86_LR; break;
    case kPc: res =  rX86_PC; break;
    case kSp: res =  rX86_SP; break;
    case kArg0: res = rX86_ARG0; break;
    case kArg1: res = rX86_ARG1; break;
    case kArg2: res = rX86_ARG2; break;
    case kArg3: res = rX86_ARG3; break;
    case kFArg0: res = rX86_FARG0; break;
    case kFArg1: res = rX86_FARG1; break;
    case kFArg2: res = rX86_FARG2; break;
    case kFArg3: res = rX86_FARG3; break;
    case kRet0: res = rX86_RET0; break;
    case kRet1: res = rX86_RET1; break;
    case kInvokeTgt: res = rX86_INVOKE_TGT; break;
    case kCount: res = rX86_COUNT; break;
  }
  return res;
}

// Create a double from a pair of singles.
int X86Mir2Lir::S2d(int low_reg, int high_reg) {
  return X86_S2D(low_reg, high_reg);
}

// Return mask to strip off fp reg flags and bias.
uint32_t X86Mir2Lir::FpRegMask() {
  return X86_FP_REG_MASK;
}

// True if both regs single, both core or both double.
bool X86Mir2Lir::SameRegType(int reg1, int reg2) {
  return (X86_REGTYPE(reg1) == X86_REGTYPE(reg2));
}

/*
 * Decode the register id.
 */
uint64_t X86Mir2Lir::GetRegMaskCommon(int reg) {
  uint64_t seed;
  int shift;
  int reg_id;

  reg_id = reg & 0xf;
  /* Double registers in x86 are just a single FP register */
  seed = 1;
  /* FP register starts at bit position 16 */
  shift = X86_FPREG(reg) ? kX86FPReg0 : 0;
  /* Expand the double register id into single offset */
  shift += reg_id;
  return (seed << shift);
}

uint64_t X86Mir2Lir::GetPCUseDefEncoding() {
  /*
   * FIXME: might make sense to use a virtual resource encoding bit for pc.  Might be
   * able to clean up some of the x86/Arm_Mips differences
   */
  LOG(FATAL) << "Unexpected call to GetPCUseDefEncoding for x86";
  return 0ULL;
}

void X86Mir2Lir::SetupTargetResourceMasks(LIR* lir) {
  DCHECK_EQ(cu_->instruction_set, kX86);

  // X86-specific resource map setup here.
  uint64_t flags = X86Mir2Lir::EncodingMap[lir->opcode].flags;

  if (flags & REG_USE_SP) {
    lir->use_mask |= ENCODE_X86_REG_SP;
  }

  if (flags & REG_DEF_SP) {
    lir->def_mask |= ENCODE_X86_REG_SP;
  }

  if (flags & REG_DEFA) {
    SetupRegMask(&lir->def_mask, rAX);
  }

  if (flags & REG_DEFD) {
    SetupRegMask(&lir->def_mask, rDX);
  }
  if (flags & REG_USEA) {
    SetupRegMask(&lir->use_mask, rAX);
  }

  if (flags & REG_USEC) {
    SetupRegMask(&lir->use_mask, rCX);
  }

  if (flags & REG_USED) {
    SetupRegMask(&lir->use_mask, rDX);
  }
}

/* For dumping instructions */
static const char* x86RegName[] = {
  "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
};

static const char* x86CondName[] = {
  "O",
  "NO",
  "B/NAE/C",
  "NB/AE/NC",
  "Z/EQ",
  "NZ/NE",
  "BE/NA",
  "NBE/A",
  "S",
  "NS",
  "P/PE",
  "NP/PO",
  "L/NGE",
  "NL/GE",
  "LE/NG",
  "NLE/G"
};

/*
 * Interpret a format string and build a string no longer than size
 * See format key in Assemble.cc.
 */
std::string X86Mir2Lir::BuildInsnString(const char *fmt, LIR *lir, unsigned char* base_addr) {
  std::string buf;
  size_t i = 0;
  size_t fmt_len = strlen(fmt);
  while (i < fmt_len) {
    if (fmt[i] != '!') {
      buf += fmt[i];
      i++;
    } else {
      i++;
      DCHECK_LT(i, fmt_len);
      char operand_number_ch = fmt[i];
      i++;
      if (operand_number_ch == '!') {
        buf += "!";
      } else {
        int operand_number = operand_number_ch - '0';
        DCHECK_LT(operand_number, 6);  // Expect upto 6 LIR operands.
        DCHECK_LT(i, fmt_len);
        int operand = lir->operands[operand_number];
        switch (fmt[i]) {
          case 'c':
            DCHECK_LT(static_cast<size_t>(operand), sizeof(x86CondName));
            buf += x86CondName[operand];
            break;
          case 'd':
            buf += StringPrintf("%d", operand);
            break;
          case 'p': {
            SwitchTable *tab_rec = reinterpret_cast<SwitchTable*>(operand);
            buf += StringPrintf("0x%08x", tab_rec->offset);
            break;
          }
          case 'r':
            if (X86_FPREG(operand) || X86_DOUBLEREG(operand)) {
              int fp_reg = operand & X86_FP_REG_MASK;
              buf += StringPrintf("xmm%d", fp_reg);
            } else {
              DCHECK_LT(static_cast<size_t>(operand), sizeof(x86RegName));
              buf += x86RegName[operand];
            }
            break;
          case 't':
            buf += StringPrintf("0x%08x (L%p)",
                                reinterpret_cast<uint32_t>(base_addr)
                                + lir->offset + operand, lir->target);
            break;
          default:
            buf += StringPrintf("DecodeError '%c'", fmt[i]);
            break;
        }
        i++;
      }
    }
  }
  return buf;
}

void X86Mir2Lir::DumpResourceMask(LIR *x86LIR, uint64_t mask, const char *prefix) {
  char buf[256];
  buf[0] = 0;

  if (mask == ENCODE_ALL) {
    strcpy(buf, "all");
  } else {
    char num[8];
    int i;

    for (i = 0; i < kX86RegEnd; i++) {
      if (mask & (1ULL << i)) {
        sprintf(num, "%d ", i);
        strcat(buf, num);
      }
    }

    if (mask & ENCODE_CCODE) {
      strcat(buf, "cc ");
    }
    /* Memory bits */
    if (x86LIR && (mask & ENCODE_DALVIK_REG)) {
      sprintf(buf + strlen(buf), "dr%d%s", x86LIR->alias_info & 0xffff,
              (x86LIR->alias_info & 0x80000000) ? "(+1)" : "");
    }
    if (mask & ENCODE_LITERAL) {
      strcat(buf, "lit ");
    }

    if (mask & ENCODE_HEAP_REF) {
      strcat(buf, "heap ");
    }
    if (mask & ENCODE_MUST_NOT_ALIAS) {
      strcat(buf, "noalias ");
    }
  }
  if (buf[0]) {
    LOG(INFO) << prefix << ": " <<  buf;
  }
}

void X86Mir2Lir::AdjustSpillMask() {
  // Adjustment for LR spilling, x86 has no LR so nothing to do here
  core_spill_mask_ |= (1 << rRET);
  num_core_spills_++;
}

/*
 * Mark a callee-save fp register as promoted.  Note that
 * vpush/vpop uses contiguous register lists so we must
 * include any holes in the mask.  Associate holes with
 * Dalvik register INVALID_VREG (0xFFFFU).
 */
void X86Mir2Lir::MarkPreservedSingle(int v_reg, int reg) {
  UNIMPLEMENTED(WARNING) << "MarkPreservedSingle";
#if 0
  LOG(FATAL) << "No support yet for promoted FP regs";
#endif
}

void X86Mir2Lir::FlushRegWide(int reg1, int reg2) {
  RegisterInfo* info1 = GetRegInfo(reg1);
  RegisterInfo* info2 = GetRegInfo(reg2);
  DCHECK(info1 && info2 && info1->pair && info2->pair &&
         (info1->partner == info2->reg) &&
         (info2->partner == info1->reg));
  if ((info1->live && info1->dirty) || (info2->live && info2->dirty)) {
    if (!(info1->is_temp && info2->is_temp)) {
      /* Should not happen.  If it does, there's a problem in eval_loc */
      LOG(FATAL) << "Long half-temp, half-promoted";
    }

    info1->dirty = false;
    info2->dirty = false;
    if (mir_graph_->SRegToVReg(info2->s_reg) < mir_graph_->SRegToVReg(info1->s_reg))
      info1 = info2;
    int v_reg = mir_graph_->SRegToVReg(info1->s_reg);
    StoreBaseDispWide(rX86_SP, VRegOffset(v_reg), info1->reg, info1->partner);
  }
}

void X86Mir2Lir::FlushReg(int reg) {
  RegisterInfo* info = GetRegInfo(reg);
  if (info->live && info->dirty) {
    info->dirty = false;
    int v_reg = mir_graph_->SRegToVReg(info->s_reg);
    StoreBaseDisp(rX86_SP, VRegOffset(v_reg), reg, kWord);
  }
}

/* Give access to the target-dependent FP register encoding to common code */
bool X86Mir2Lir::IsFpReg(int reg) {
  return X86_FPREG(reg);
}

/* Clobber all regs that might be used by an external C call */
void X86Mir2Lir::ClobberCalleeSave() {
  Clobber(rAX);
  Clobber(rCX);
  Clobber(rDX);
}

RegLocation X86Mir2Lir::GetReturnWideAlt() {
  RegLocation res = LocCReturnWide();
  CHECK(res.low_reg == rAX);
  CHECK(res.high_reg == rDX);
  Clobber(rAX);
  Clobber(rDX);
  MarkInUse(rAX);
  MarkInUse(rDX);
  MarkPair(res.low_reg, res.high_reg);
  return res;
}

RegLocation X86Mir2Lir::GetReturnAlt() {
  RegLocation res = LocCReturn();
  res.low_reg = rDX;
  Clobber(rDX);
  MarkInUse(rDX);
  return res;
}

X86Mir2Lir::RegisterInfo* X86Mir2Lir::GetRegInfo(int reg) {
  return X86_FPREG(reg) ? &reg_pool_->FPRegs[reg & X86_FP_REG_MASK]
                    : &reg_pool_->core_regs[reg];
}

/* To be used when explicitly managing register use */
void X86Mir2Lir::LockCallTemps() {
  LockTemp(rX86_ARG0);
  LockTemp(rX86_ARG1);
  LockTemp(rX86_ARG2);
  LockTemp(rX86_ARG3);
}

/* To be used when explicitly managing register use */
void X86Mir2Lir::FreeCallTemps() {
  FreeTemp(rX86_ARG0);
  FreeTemp(rX86_ARG1);
  FreeTemp(rX86_ARG2);
  FreeTemp(rX86_ARG3);
}

void X86Mir2Lir::GenMemBarrier(MemBarrierKind barrier_kind) {
#if ANDROID_SMP != 0
  // TODO: optimize fences
  NewLIR0(kX86Mfence);
#endif
}
/*
 * Alloc a pair of core registers, or a double.  Low reg in low byte,
 * high reg in next byte.
 */
int X86Mir2Lir::AllocTypedTempPair(bool fp_hint,
                          int reg_class) {
  int high_reg;
  int low_reg;
  int res = 0;

  if (((reg_class == kAnyReg) && fp_hint) || (reg_class == kFPReg)) {
    low_reg = AllocTempDouble();
    high_reg = low_reg + 1;
    res = (low_reg & 0xff) | ((high_reg & 0xff) << 8);
    return res;
  }

  low_reg = AllocTemp();
  high_reg = AllocTemp();
  res = (low_reg & 0xff) | ((high_reg & 0xff) << 8);
  return res;
}

int X86Mir2Lir::AllocTypedTemp(bool fp_hint, int reg_class) {
  if (((reg_class == kAnyReg) && fp_hint) || (reg_class == kFPReg)) {
    return AllocTempFloat();
  }
  return AllocTemp();
}

void X86Mir2Lir::CompilerInitializeRegAlloc() {
  int num_regs = sizeof(core_regs)/sizeof(*core_regs);
  int num_reserved = sizeof(ReservedRegs)/sizeof(*ReservedRegs);
  int num_temps = sizeof(core_temps)/sizeof(*core_temps);
  int num_fp_regs = sizeof(FpRegs)/sizeof(*FpRegs);
  int num_fp_temps = sizeof(fp_temps)/sizeof(*fp_temps);
  reg_pool_ = static_cast<RegisterPool*>(arena_->Alloc(sizeof(*reg_pool_),
                                                       ArenaAllocator::kAllocRegAlloc));
  reg_pool_->num_core_regs = num_regs;
  reg_pool_->core_regs =
      static_cast<RegisterInfo*>(arena_->Alloc(num_regs * sizeof(*reg_pool_->core_regs),
                                               ArenaAllocator::kAllocRegAlloc));
  reg_pool_->num_fp_regs = num_fp_regs;
  reg_pool_->FPRegs =
      static_cast<RegisterInfo *>(arena_->Alloc(num_fp_regs * sizeof(*reg_pool_->FPRegs),
                                                ArenaAllocator::kAllocRegAlloc));
  CompilerInitPool(reg_pool_->core_regs, core_regs, reg_pool_->num_core_regs);
  CompilerInitPool(reg_pool_->FPRegs, FpRegs, reg_pool_->num_fp_regs);
  // Keep special registers from being allocated
  for (int i = 0; i < num_reserved; i++) {
    MarkInUse(ReservedRegs[i]);
  }
  // Mark temp regs - all others not in use can be used for promotion
  for (int i = 0; i < num_temps; i++) {
    MarkTemp(core_temps[i]);
  }
  for (int i = 0; i < num_fp_temps; i++) {
    MarkTemp(fp_temps[i]);
  }
}

void X86Mir2Lir::FreeRegLocTemps(RegLocation rl_keep,
                     RegLocation rl_free) {
  if ((rl_free.low_reg != rl_keep.low_reg) && (rl_free.low_reg != rl_keep.high_reg) &&
      (rl_free.high_reg != rl_keep.low_reg) && (rl_free.high_reg != rl_keep.high_reg)) {
    // No overlap, free both
    FreeTemp(rl_free.low_reg);
    FreeTemp(rl_free.high_reg);
  }
}

void X86Mir2Lir::SpillCoreRegs() {
  if (num_core_spills_ == 0) {
    return;
  }
  // Spill mask not including fake return address register
  uint32_t mask = core_spill_mask_ & ~(1 << rRET);
  int offset = frame_size_ - (4 * num_core_spills_);
  for (int reg = 0; mask; mask >>= 1, reg++) {
    if (mask & 0x1) {
      StoreWordDisp(rX86_SP, offset, reg);
      offset += 4;
    }
  }
}

void X86Mir2Lir::UnSpillCoreRegs() {
  if (num_core_spills_ == 0) {
    return;
  }
  // Spill mask not including fake return address register
  uint32_t mask = core_spill_mask_ & ~(1 << rRET);
  int offset = frame_size_ - (4 * num_core_spills_);
  for (int reg = 0; mask; mask >>= 1, reg++) {
    if (mask & 0x1) {
      LoadWordDisp(rX86_SP, offset, reg);
      offset += 4;
    }
  }
}

bool X86Mir2Lir::IsUnconditionalBranch(LIR* lir) {
  return (lir->opcode == kX86Jmp8 || lir->opcode == kX86Jmp32);
}

X86Mir2Lir::X86Mir2Lir(CompilationUnit* cu, MIRGraph* mir_graph, ArenaAllocator* arena)
    : Mir2Lir(cu, mir_graph, arena) {
  for (int i = 0; i < kX86Last; i++) {
    if (X86Mir2Lir::EncodingMap[i].opcode != i) {
      LOG(FATAL) << "Encoding order for " << X86Mir2Lir::EncodingMap[i].name
                 << " is wrong: expecting " << i << ", seeing "
                 << static_cast<int>(X86Mir2Lir::EncodingMap[i].opcode);
    }
  }
}

Mir2Lir* X86CodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                          ArenaAllocator* const arena) {
  return new X86Mir2Lir(cu, mir_graph, arena);
}

// Not used in x86
int X86Mir2Lir::LoadHelper(ThreadOffset offset) {
  LOG(FATAL) << "Unexpected use of LoadHelper in x86";
  return INVALID_REG;
}

uint64_t X86Mir2Lir::GetTargetInstFlags(int opcode) {
  return X86Mir2Lir::EncodingMap[opcode].flags;
}

const char* X86Mir2Lir::GetTargetInstName(int opcode) {
  return X86Mir2Lir::EncodingMap[opcode].name;
}

const char* X86Mir2Lir::GetTargetInstFmt(int opcode) {
  return X86Mir2Lir::EncodingMap[opcode].fmt;
}

}  // namespace art
