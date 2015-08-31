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

#include "dex/compiler_internals.h"
#include "dex_file-inl.h"
#include "gc_map.h"
#include "mapping_table.h"
#include "mir_to_lir-inl.h"
#include "verifier/dex_gc_map.h"
#include "verifier/method_verifier.h"

namespace art {

bool Mir2Lir::IsInexpensiveConstant(RegLocation rl_src) {
  bool res = false;
  if (rl_src.is_const) {
    if (rl_src.wide) {
      if (rl_src.fp) {
         res = InexpensiveConstantDouble(mir_graph_->ConstantValueWide(rl_src));
      } else {
         res = InexpensiveConstantLong(mir_graph_->ConstantValueWide(rl_src));
      }
    } else {
      if (rl_src.fp) {
         res = InexpensiveConstantFloat(mir_graph_->ConstantValue(rl_src));
      } else {
         res = InexpensiveConstantInt(mir_graph_->ConstantValue(rl_src));
      }
    }
  }
  return res;
}

void Mir2Lir::MarkSafepointPC(LIR* inst) {
  inst->def_mask = ENCODE_ALL;
  LIR* safepoint_pc = NewLIR0(kPseudoSafepointPC);
  DCHECK_EQ(safepoint_pc->def_mask, ENCODE_ALL);
}

bool Mir2Lir::FastInstance(uint32_t field_idx, int& field_offset, bool& is_volatile, bool is_put) {
  return cu_->compiler_driver->ComputeInstanceFieldInfo(
      field_idx, mir_graph_->GetCurrentDexCompilationUnit(), field_offset, is_volatile, is_put);
}

/* Convert an instruction to a NOP */
void Mir2Lir::NopLIR(LIR* lir) {
  lir->flags.is_nop = true;
}

void Mir2Lir::SetMemRefType(LIR* lir, bool is_load, int mem_type) {
  uint64_t *mask_ptr;
  uint64_t mask = ENCODE_MEM;
  DCHECK(GetTargetInstFlags(lir->opcode) & (IS_LOAD | IS_STORE));
  if (is_load) {
    mask_ptr = &lir->use_mask;
  } else {
    mask_ptr = &lir->def_mask;
  }
  /* Clear out the memref flags */
  *mask_ptr &= ~mask;
  /* ..and then add back the one we need */
  switch (mem_type) {
    case kLiteral:
      DCHECK(is_load);
      *mask_ptr |= ENCODE_LITERAL;
      break;
    case kDalvikReg:
      *mask_ptr |= ENCODE_DALVIK_REG;
      break;
    case kHeapRef:
      *mask_ptr |= ENCODE_HEAP_REF;
      break;
    case kMustNotAlias:
      /* Currently only loads can be marked as kMustNotAlias */
      DCHECK(!(GetTargetInstFlags(lir->opcode) & IS_STORE));
      *mask_ptr |= ENCODE_MUST_NOT_ALIAS;
      break;
    default:
      LOG(FATAL) << "Oat: invalid memref kind - " << mem_type;
  }
}

/*
 * Mark load/store instructions that access Dalvik registers through the stack.
 */
void Mir2Lir::AnnotateDalvikRegAccess(LIR* lir, int reg_id, bool is_load,
                                      bool is64bit) {
  SetMemRefType(lir, is_load, kDalvikReg);

  /*
   * Store the Dalvik register id in alias_info. Mark the MSB if it is a 64-bit
   * access.
   */
  lir->alias_info = ENCODE_ALIAS_INFO(reg_id, is64bit);
}

/*
 * Debugging macros
 */
#define DUMP_RESOURCE_MASK(X)

/* Pretty-print a LIR instruction */
void Mir2Lir::DumpLIRInsn(LIR* lir, unsigned char* base_addr) {
  int offset = lir->offset;
  int dest = lir->operands[0];
  const bool dump_nop = (cu_->enable_debug & (1 << kDebugShowNops));

  /* Handle pseudo-ops individually, and all regular insns as a group */
  switch (lir->opcode) {
    case kPseudoMethodEntry:
      LOG(INFO) << "-------- method entry "
                << PrettyMethod(cu_->method_idx, *cu_->dex_file);
      break;
    case kPseudoMethodExit:
      LOG(INFO) << "-------- Method_Exit";
      break;
    case kPseudoBarrier:
      LOG(INFO) << "-------- BARRIER";
      break;
    case kPseudoEntryBlock:
      LOG(INFO) << "-------- entry offset: 0x" << std::hex << dest;
      break;
    case kPseudoDalvikByteCodeBoundary:
      if (lir->operands[0] == 0) {
         lir->operands[0] = reinterpret_cast<uintptr_t>("No instruction string");
      }
      LOG(INFO) << "-------- dalvik offset: 0x" << std::hex
                << lir->dalvik_offset << " @ " << reinterpret_cast<char*>(lir->operands[0]);
      break;
    case kPseudoExitBlock:
      LOG(INFO) << "-------- exit offset: 0x" << std::hex << dest;
      break;
    case kPseudoPseudoAlign4:
      LOG(INFO) << reinterpret_cast<uintptr_t>(base_addr) + offset << " (0x" << std::hex
                << offset << "): .align4";
      break;
    case kPseudoEHBlockLabel:
      LOG(INFO) << "Exception_Handling:";
      break;
    case kPseudoTargetLabel:
    case kPseudoNormalBlockLabel:
      LOG(INFO) << "L" << reinterpret_cast<void*>(lir) << ":";
      break;
    case kPseudoThrowTarget:
      LOG(INFO) << "LT" << reinterpret_cast<void*>(lir) << ":";
      break;
    case kPseudoIntrinsicRetry:
      LOG(INFO) << "IR" << reinterpret_cast<void*>(lir) << ":";
      break;
    case kPseudoSuspendTarget:
      LOG(INFO) << "LS" << reinterpret_cast<void*>(lir) << ":";
      break;
    case kPseudoSafepointPC:
      LOG(INFO) << "LsafepointPC_0x" << std::hex << lir->offset << "_" << lir->dalvik_offset << ":";
      break;
    case kPseudoExportedPC:
      LOG(INFO) << "LexportedPC_0x" << std::hex << lir->offset << "_" << lir->dalvik_offset << ":";
      break;
    case kPseudoCaseLabel:
      LOG(INFO) << "LC" << reinterpret_cast<void*>(lir) << ": Case target 0x"
                << std::hex << lir->operands[0] << "|" << std::dec <<
        lir->operands[0];
      break;
    default:
      if (lir->flags.is_nop && !dump_nop) {
        break;
      } else {
        std::string op_name(BuildInsnString(GetTargetInstName(lir->opcode),
                                               lir, base_addr));
        std::string op_operands(BuildInsnString(GetTargetInstFmt(lir->opcode),
                                                    lir, base_addr));
        LOG(INFO) << StringPrintf("%05x: %-9s%s%s",
                                  reinterpret_cast<unsigned int>(base_addr + offset),
                                  op_name.c_str(), op_operands.c_str(),
                                  lir->flags.is_nop ? "(nop)" : "");
      }
      break;
  }

  if (lir->use_mask && (!lir->flags.is_nop || dump_nop)) {
    DUMP_RESOURCE_MASK(DumpResourceMask(lir, lir->use_mask, "use"));
  }
  if (lir->def_mask && (!lir->flags.is_nop || dump_nop)) {
    DUMP_RESOURCE_MASK(DumpResourceMask(lir, lir->def_mask, "def"));
  }
}

void Mir2Lir::DumpPromotionMap() {
  int num_regs = cu_->num_dalvik_registers + cu_->num_compiler_temps + 1;
  for (int i = 0; i < num_regs; i++) {
    PromotionMap v_reg_map = promotion_map_[i];
    std::string buf;
    if (v_reg_map.fp_location == kLocPhysReg) {
      StringAppendF(&buf, " : s%d", v_reg_map.FpReg & FpRegMask());
    }

    std::string buf3;
    if (i < cu_->num_dalvik_registers) {
      StringAppendF(&buf3, "%02d", i);
    } else if (i == mir_graph_->GetMethodSReg()) {
      buf3 = "Method*";
    } else {
      StringAppendF(&buf3, "ct%d", i - cu_->num_dalvik_registers);
    }

    LOG(INFO) << StringPrintf("V[%s] -> %s%d%s", buf3.c_str(),
                              v_reg_map.core_location == kLocPhysReg ?
                              "r" : "SP+", v_reg_map.core_location == kLocPhysReg ?
                              v_reg_map.core_reg : SRegOffset(i),
                              buf.c_str());
  }
}

/* Dump a mapping table */
void Mir2Lir::DumpMappingTable(const char* table_name, const std::string& descriptor,
                               const std::string& name, const std::string& signature,
                               const std::vector<uint32_t>& v) {
  if (v.size() > 0) {
    std::string line(StringPrintf("\n  %s %s%s_%s_table[%zu] = {", table_name,
                     descriptor.c_str(), name.c_str(), signature.c_str(), v.size()));
    std::replace(line.begin(), line.end(), ';', '_');
    LOG(INFO) << line;
    for (uint32_t i = 0; i < v.size(); i+=2) {
      line = StringPrintf("    {0x%05x, 0x%04x},", v[i], v[i+1]);
      LOG(INFO) << line;
    }
    LOG(INFO) <<"  };\n\n";
  }
}

/* Dump instructions and constant pool contents */
void Mir2Lir::CodegenDump() {
  LOG(INFO) << "Dumping LIR insns for "
            << PrettyMethod(cu_->method_idx, *cu_->dex_file);
  LIR* lir_insn;
  int insns_size = cu_->code_item->insns_size_in_code_units_;

  LOG(INFO) << "Regs (excluding ins) : " << cu_->num_regs;
  LOG(INFO) << "Ins          : " << cu_->num_ins;
  LOG(INFO) << "Outs         : " << cu_->num_outs;
  LOG(INFO) << "CoreSpills       : " << num_core_spills_;
  LOG(INFO) << "FPSpills       : " << num_fp_spills_;
  LOG(INFO) << "CompilerTemps    : " << cu_->num_compiler_temps;
  LOG(INFO) << "Frame size       : " << frame_size_;
  LOG(INFO) << "code size is " << total_size_ <<
    " bytes, Dalvik size is " << insns_size * 2;
  LOG(INFO) << "expansion factor: "
            << static_cast<float>(total_size_) / static_cast<float>(insns_size * 2);
  DumpPromotionMap();
  for (lir_insn = first_lir_insn_; lir_insn != NULL; lir_insn = lir_insn->next) {
    DumpLIRInsn(lir_insn, 0);
  }
  for (lir_insn = literal_list_; lir_insn != NULL; lir_insn = lir_insn->next) {
    LOG(INFO) << StringPrintf("%x (%04x): .word (%#x)", lir_insn->offset, lir_insn->offset,
                              lir_insn->operands[0]);
  }

  const DexFile::MethodId& method_id =
      cu_->dex_file->GetMethodId(cu_->method_idx);
  std::string signature(cu_->dex_file->GetMethodSignature(method_id));
  std::string name(cu_->dex_file->GetMethodName(method_id));
  std::string descriptor(cu_->dex_file->GetMethodDeclaringClassDescriptor(method_id));

  // Dump mapping tables
  DumpMappingTable("PC2Dex_MappingTable", descriptor, name, signature, pc2dex_mapping_table_);
  DumpMappingTable("Dex2PC_MappingTable", descriptor, name, signature, dex2pc_mapping_table_);
}

/*
 * Search the existing constants in the literal pool for an exact or close match
 * within specified delta (greater or equal to 0).
 */
LIR* Mir2Lir::ScanLiteralPool(LIR* data_target, int value, unsigned int delta) {
  while (data_target) {
    if ((static_cast<unsigned>(value - data_target->operands[0])) <= delta)
      return data_target;
    data_target = data_target->next;
  }
  return NULL;
}

/* Search the existing constants in the literal pool for an exact wide match */
LIR* Mir2Lir::ScanLiteralPoolWide(LIR* data_target, int val_lo, int val_hi) {
  bool lo_match = false;
  LIR* lo_target = NULL;
  while (data_target) {
    if (lo_match && (data_target->operands[0] == val_hi)) {
      // Record high word in case we need to expand this later.
      lo_target->operands[1] = val_hi;
      return lo_target;
    }
    lo_match = false;
    if (data_target->operands[0] == val_lo) {
      lo_match = true;
      lo_target = data_target;
    }
    data_target = data_target->next;
  }
  return NULL;
}

/*
 * The following are building blocks to insert constants into the pool or
 * instruction streams.
 */

/* Add a 32-bit constant to the constant pool */
LIR* Mir2Lir::AddWordData(LIR* *constant_list_p, int value) {
  /* Add the constant to the literal pool */
  if (constant_list_p) {
    LIR* new_value = static_cast<LIR*>(arena_->Alloc(sizeof(LIR), ArenaAllocator::kAllocData));
    new_value->operands[0] = value;
    new_value->next = *constant_list_p;
    *constant_list_p = new_value;
    return new_value;
  }
  return NULL;
}

/* Add a 64-bit constant to the constant pool or mixed with code */
LIR* Mir2Lir::AddWideData(LIR* *constant_list_p, int val_lo, int val_hi) {
  AddWordData(constant_list_p, val_hi);
  return AddWordData(constant_list_p, val_lo);
}

static void PushWord(std::vector<uint8_t>&buf, int data) {
  buf.push_back(data & 0xff);
  buf.push_back((data >> 8) & 0xff);
  buf.push_back((data >> 16) & 0xff);
  buf.push_back((data >> 24) & 0xff);
}

static void AlignBuffer(std::vector<uint8_t>&buf, size_t offset) {
  while (buf.size() < offset) {
    buf.push_back(0);
  }
}

/* Write the literal pool to the output stream */
void Mir2Lir::InstallLiteralPools() {
  AlignBuffer(code_buffer_, data_offset_);
  LIR* data_lir = literal_list_;
  while (data_lir != NULL) {
    PushWord(code_buffer_, data_lir->operands[0]);
    data_lir = NEXT_LIR(data_lir);
  }
  // Push code and method literals, record offsets for the compiler to patch.
  data_lir = code_literal_list_;
  while (data_lir != NULL) {
    uint32_t target = data_lir->operands[0];
    cu_->compiler_driver->AddCodePatch(cu_->dex_file,
                                       cu_->class_def_idx,
                                       cu_->method_idx,
                                       cu_->invoke_type,
                                       target,
                                       static_cast<InvokeType>(data_lir->operands[1]),
                                       code_buffer_.size());
    const DexFile::MethodId& id = cu_->dex_file->GetMethodId(target);
    // unique based on target to ensure code deduplication works
    uint32_t unique_patch_value = reinterpret_cast<uint32_t>(&id);
    PushWord(code_buffer_, unique_patch_value);
    data_lir = NEXT_LIR(data_lir);
  }
  data_lir = method_literal_list_;
  while (data_lir != NULL) {
    uint32_t target = data_lir->operands[0];
    cu_->compiler_driver->AddMethodPatch(cu_->dex_file,
                                         cu_->class_def_idx,
                                         cu_->method_idx,
                                         cu_->invoke_type,
                                         target,
                                         static_cast<InvokeType>(data_lir->operands[1]),
                                         code_buffer_.size());
    const DexFile::MethodId& id = cu_->dex_file->GetMethodId(target);
    // unique based on target to ensure code deduplication works
    uint32_t unique_patch_value = reinterpret_cast<uint32_t>(&id);
    PushWord(code_buffer_, unique_patch_value);
    data_lir = NEXT_LIR(data_lir);
  }
}

/* Write the switch tables to the output stream */
void Mir2Lir::InstallSwitchTables() {
  GrowableArray<SwitchTable*>::Iterator iterator(&switch_tables_);
  while (true) {
    Mir2Lir::SwitchTable* tab_rec = iterator.Next();
    if (tab_rec == NULL) break;
    AlignBuffer(code_buffer_, tab_rec->offset);
    /*
     * For Arm, our reference point is the address of the bx
     * instruction that does the launch, so we have to subtract
     * the auto pc-advance.  For other targets the reference point
     * is a label, so we can use the offset as-is.
     */
    int bx_offset = INVALID_OFFSET;
    switch (cu_->instruction_set) {
      case kThumb2:
        bx_offset = tab_rec->anchor->offset + 4;
        break;
      case kX86:
        bx_offset = 0;
        break;
      case kMips:
        bx_offset = tab_rec->anchor->offset;
        break;
      default: LOG(FATAL) << "Unexpected instruction set: " << cu_->instruction_set;
    }
    if (cu_->verbose) {
      LOG(INFO) << "Switch table for offset 0x" << std::hex << bx_offset;
    }
    if (tab_rec->table[0] == Instruction::kSparseSwitchSignature) {
      const int* keys = reinterpret_cast<const int*>(&(tab_rec->table[2]));
      for (int elems = 0; elems < tab_rec->table[1]; elems++) {
        int disp = tab_rec->targets[elems]->offset - bx_offset;
        if (cu_->verbose) {
          LOG(INFO) << "  Case[" << elems << "] key: 0x"
                    << std::hex << keys[elems] << ", disp: 0x"
                    << std::hex << disp;
        }
        PushWord(code_buffer_, keys[elems]);
        PushWord(code_buffer_,
          tab_rec->targets[elems]->offset - bx_offset);
      }
    } else {
      DCHECK_EQ(static_cast<int>(tab_rec->table[0]),
                static_cast<int>(Instruction::kPackedSwitchSignature));
      for (int elems = 0; elems < tab_rec->table[1]; elems++) {
        int disp = tab_rec->targets[elems]->offset - bx_offset;
        if (cu_->verbose) {
          LOG(INFO) << "  Case[" << elems << "] disp: 0x"
                    << std::hex << disp;
        }
        PushWord(code_buffer_, tab_rec->targets[elems]->offset - bx_offset);
      }
    }
  }
}

/* Write the fill array dta to the output stream */
void Mir2Lir::InstallFillArrayData() {
  GrowableArray<FillArrayData*>::Iterator iterator(&fill_array_data_);
  while (true) {
    Mir2Lir::FillArrayData *tab_rec = iterator.Next();
    if (tab_rec == NULL) break;
    AlignBuffer(code_buffer_, tab_rec->offset);
    for (int i = 0; i < (tab_rec->size + 1) / 2; i++) {
      code_buffer_.push_back(tab_rec->table[i] & 0xFF);
      code_buffer_.push_back((tab_rec->table[i] >> 8) & 0xFF);
    }
  }
}

static int AssignLiteralOffsetCommon(LIR* lir, int offset) {
  for (; lir != NULL; lir = lir->next) {
    lir->offset = offset;
    offset += 4;
  }
  return offset;
}

// Make sure we have a code address for every declared catch entry
bool Mir2Lir::VerifyCatchEntries() {
  bool success = true;
  for (std::set<uint32_t>::const_iterator it = mir_graph_->catches_.begin();
       it != mir_graph_->catches_.end(); ++it) {
    uint32_t dex_pc = *it;
    bool found = false;
    for (size_t i = 0; i < dex2pc_mapping_table_.size(); i += 2) {
      if (dex_pc == dex2pc_mapping_table_[i+1]) {
        found = true;
        break;
      }
    }
    if (!found) {
      LOG(INFO) << "Missing native PC for catch entry @ 0x" << std::hex << dex_pc;
      success = false;
    }
  }
  // Now, try in the other direction
  for (size_t i = 0; i < dex2pc_mapping_table_.size(); i += 2) {
    uint32_t dex_pc = dex2pc_mapping_table_[i+1];
    if (mir_graph_->catches_.find(dex_pc) == mir_graph_->catches_.end()) {
      LOG(INFO) << "Unexpected catch entry @ dex pc 0x" << std::hex << dex_pc;
      success = false;
    }
  }
  if (!success) {
    LOG(INFO) << "Bad dex2pcMapping table in " << PrettyMethod(cu_->method_idx, *cu_->dex_file);
    LOG(INFO) << "Entries @ decode: " << mir_graph_->catches_.size() << ", Entries in table: "
              << dex2pc_mapping_table_.size()/2;
  }
  return success;
}


void Mir2Lir::CreateMappingTables() {
  for (LIR* tgt_lir = first_lir_insn_; tgt_lir != NULL; tgt_lir = NEXT_LIR(tgt_lir)) {
    if (!tgt_lir->flags.is_nop && (tgt_lir->opcode == kPseudoSafepointPC)) {
      pc2dex_mapping_table_.push_back(tgt_lir->offset);
      pc2dex_mapping_table_.push_back(tgt_lir->dalvik_offset);
    }
    if (!tgt_lir->flags.is_nop && (tgt_lir->opcode == kPseudoExportedPC)) {
      dex2pc_mapping_table_.push_back(tgt_lir->offset);
      dex2pc_mapping_table_.push_back(tgt_lir->dalvik_offset);
    }
  }
  if (kIsDebugBuild) {
    CHECK(VerifyCatchEntries());
  }
  CHECK_EQ(pc2dex_mapping_table_.size() & 1, 0U);
  CHECK_EQ(dex2pc_mapping_table_.size() & 1, 0U);
  uint32_t total_entries = (pc2dex_mapping_table_.size() + dex2pc_mapping_table_.size()) / 2;
  uint32_t pc2dex_entries = pc2dex_mapping_table_.size() / 2;
  encoded_mapping_table_.PushBack(total_entries);
  encoded_mapping_table_.PushBack(pc2dex_entries);
  encoded_mapping_table_.InsertBack(pc2dex_mapping_table_.begin(), pc2dex_mapping_table_.end());
  encoded_mapping_table_.InsertBack(dex2pc_mapping_table_.begin(), dex2pc_mapping_table_.end());
  if (kIsDebugBuild) {
    // Verify the encoded table holds the expected data.
    MappingTable table(&encoded_mapping_table_.GetData()[0]);
    CHECK_EQ(table.TotalSize(), total_entries);
    CHECK_EQ(table.PcToDexSize(), pc2dex_entries);
    CHECK_EQ(table.DexToPcSize(), dex2pc_mapping_table_.size() / 2);
    MappingTable::PcToDexIterator it = table.PcToDexBegin();
    for (uint32_t i = 0; i < pc2dex_mapping_table_.size(); ++i, ++it) {
      CHECK_EQ(pc2dex_mapping_table_.at(i), it.NativePcOffset());
      ++i;
      CHECK_EQ(pc2dex_mapping_table_.at(i), it.DexPc());
    }
    MappingTable::DexToPcIterator it2 = table.DexToPcBegin();
    for (uint32_t i = 0; i < dex2pc_mapping_table_.size(); ++i, ++it2) {
      CHECK_EQ(dex2pc_mapping_table_.at(i), it2.NativePcOffset());
      ++i;
      CHECK_EQ(dex2pc_mapping_table_.at(i), it2.DexPc());
    }
  }
}

class NativePcToReferenceMapBuilder {
 public:
  NativePcToReferenceMapBuilder(std::vector<uint8_t>* table,
                                size_t entries, uint32_t max_native_offset,
                                size_t references_width) : entries_(entries),
                                references_width_(references_width), in_use_(entries),
                                table_(table) {
    // Compute width in bytes needed to hold max_native_offset.
    native_offset_width_ = 0;
    while (max_native_offset != 0) {
      native_offset_width_++;
      max_native_offset >>= 8;
    }
    // Resize table and set up header.
    table->resize((EntryWidth() * entries) + sizeof(uint32_t));
    CHECK_LT(native_offset_width_, 1U << 3);
    (*table)[0] = native_offset_width_ & 7;
    CHECK_LT(references_width_, 1U << 13);
    (*table)[0] |= (references_width_ << 3) & 0xFF;
    (*table)[1] = (references_width_ >> 5) & 0xFF;
    CHECK_LT(entries, 1U << 16);
    (*table)[2] = entries & 0xFF;
    (*table)[3] = (entries >> 8) & 0xFF;
  }

  void AddEntry(uint32_t native_offset, const uint8_t* references) {
    size_t table_index = TableIndex(native_offset);
    while (in_use_[table_index]) {
      table_index = (table_index + 1) % entries_;
    }
    in_use_[table_index] = true;
    SetNativeOffset(table_index, native_offset);
    DCHECK_EQ(native_offset, GetNativeOffset(table_index));
    SetReferences(table_index, references);
  }

 private:
  size_t TableIndex(uint32_t native_offset) {
    return NativePcOffsetToReferenceMap::Hash(native_offset) % entries_;
  }

  uint32_t GetNativeOffset(size_t table_index) {
    uint32_t native_offset = 0;
    size_t table_offset = (table_index * EntryWidth()) + sizeof(uint32_t);
    for (size_t i = 0; i < native_offset_width_; i++) {
      native_offset |= (*table_)[table_offset + i] << (i * 8);
    }
    return native_offset;
  }

  void SetNativeOffset(size_t table_index, uint32_t native_offset) {
    size_t table_offset = (table_index * EntryWidth()) + sizeof(uint32_t);
    for (size_t i = 0; i < native_offset_width_; i++) {
      (*table_)[table_offset + i] = (native_offset >> (i * 8)) & 0xFF;
    }
  }

  void SetReferences(size_t table_index, const uint8_t* references) {
    size_t table_offset = (table_index * EntryWidth()) + sizeof(uint32_t);
    memcpy(&(*table_)[table_offset + native_offset_width_], references, references_width_);
  }

  size_t EntryWidth() const {
    return native_offset_width_ + references_width_;
  }

  // Number of entries in the table.
  const size_t entries_;
  // Number of bytes used to encode the reference bitmap.
  const size_t references_width_;
  // Number of bytes used to encode a native offset.
  size_t native_offset_width_;
  // Entries that are in use.
  std::vector<bool> in_use_;
  // The table we're building.
  std::vector<uint8_t>* const table_;
};

void Mir2Lir::CreateNativeGcMap() {
  const std::vector<uint32_t>& mapping_table = pc2dex_mapping_table_;
  uint32_t max_native_offset = 0;
  for (size_t i = 0; i < mapping_table.size(); i += 2) {
    uint32_t native_offset = mapping_table[i + 0];
    if (native_offset > max_native_offset) {
      max_native_offset = native_offset;
    }
  }
  MethodReference method_ref(cu_->dex_file, cu_->method_idx);
  const std::vector<uint8_t>* gc_map_raw = verifier::MethodVerifier::GetDexGcMap(method_ref);
  verifier::DexPcToReferenceMap dex_gc_map(&(*gc_map_raw)[4], gc_map_raw->size() - 4);
  // Compute native offset to references size.
  NativePcToReferenceMapBuilder native_gc_map_builder(&native_gc_map_,
                                                      mapping_table.size() / 2, max_native_offset,
                                                      dex_gc_map.RegWidth());

  for (size_t i = 0; i < mapping_table.size(); i += 2) {
    uint32_t native_offset = mapping_table[i + 0];
    uint32_t dex_pc = mapping_table[i + 1];
    const uint8_t* references = dex_gc_map.FindBitMap(dex_pc, false);
    CHECK(references != NULL) << "Missing ref for dex pc 0x" << std::hex << dex_pc;
    native_gc_map_builder.AddEntry(native_offset, references);
  }
}

/* Determine the offset of each literal field */
int Mir2Lir::AssignLiteralOffset(int offset) {
  offset = AssignLiteralOffsetCommon(literal_list_, offset);
  offset = AssignLiteralOffsetCommon(code_literal_list_, offset);
  offset = AssignLiteralOffsetCommon(method_literal_list_, offset);
  return offset;
}

int Mir2Lir::AssignSwitchTablesOffset(int offset) {
  GrowableArray<SwitchTable*>::Iterator iterator(&switch_tables_);
  while (true) {
    Mir2Lir::SwitchTable *tab_rec = iterator.Next();
    if (tab_rec == NULL) break;
    tab_rec->offset = offset;
    if (tab_rec->table[0] == Instruction::kSparseSwitchSignature) {
      offset += tab_rec->table[1] * (sizeof(int) * 2);
    } else {
      DCHECK_EQ(static_cast<int>(tab_rec->table[0]),
                static_cast<int>(Instruction::kPackedSwitchSignature));
      offset += tab_rec->table[1] * sizeof(int);
    }
  }
  return offset;
}

int Mir2Lir::AssignFillArrayDataOffset(int offset) {
  GrowableArray<FillArrayData*>::Iterator iterator(&fill_array_data_);
  while (true) {
    Mir2Lir::FillArrayData *tab_rec = iterator.Next();
    if (tab_rec == NULL) break;
    tab_rec->offset = offset;
    offset += tab_rec->size;
    // word align
    offset = (offset + 3) & ~3;
    }
  return offset;
}

// LIR offset assignment.
int Mir2Lir::AssignInsnOffsets() {
  LIR* lir;
  int offset = 0;

  for (lir = first_lir_insn_; lir != NULL; lir = NEXT_LIR(lir)) {
    lir->offset = offset;
    if (lir->opcode >= 0) {
      if (!lir->flags.is_nop) {
        offset += lir->flags.size;
      }
    } else if (lir->opcode == kPseudoPseudoAlign4) {
      if (offset & 0x2) {
        offset += 2;
        lir->operands[0] = 1;
      } else {
        lir->operands[0] = 0;
      }
    }
    /* Pseudo opcodes don't consume space */
  }

  return offset;
}

/*
 * Walk the compilation unit and assign offsets to instructions
 * and literals and compute the total size of the compiled unit.
 */
void Mir2Lir::AssignOffsets() {
  int offset = AssignInsnOffsets();

  /* Const values have to be word aligned */
  offset = (offset + 3) & ~3;

  /* Set up offsets for literals */
  data_offset_ = offset;

  offset = AssignLiteralOffset(offset);

  offset = AssignSwitchTablesOffset(offset);

  offset = AssignFillArrayDataOffset(offset);

  total_size_ = offset;
}

/*
 * Go over each instruction in the list and calculate the offset from the top
 * before sending them off to the assembler. If out-of-range branch distance is
 * seen rearrange the instructions a bit to correct it.
 */
void Mir2Lir::AssembleLIR() {
  AssignOffsets();
  int assembler_retries = 0;
  /*
   * Assemble here.  Note that we generate code with optimistic assumptions
   * and if found now to work, we'll have to redo the sequence and retry.
   */

  while (true) {
    AssemblerStatus res = AssembleInstructions(0);
    if (res == kSuccess) {
      break;
    } else {
      assembler_retries++;
      if (assembler_retries > MAX_ASSEMBLER_RETRIES) {
        CodegenDump();
        LOG(FATAL) << "Assembler error - too many retries";
      }
      // Redo offsets and try again
      AssignOffsets();
      code_buffer_.clear();
    }
  }

  // Install literals
  InstallLiteralPools();

  // Install switch tables
  InstallSwitchTables();

  // Install fill array data
  InstallFillArrayData();

  // Create the mapping table and native offset to reference map.
  CreateMappingTables();

  CreateNativeGcMap();
}

/*
 * Insert a kPseudoCaseLabel at the beginning of the Dalvik
 * offset vaddr.  This label will be used to fix up the case
 * branch table during the assembly phase.  Be sure to set
 * all resource flags on this to prevent code motion across
 * target boundaries.  KeyVal is just there for debugging.
 */
LIR* Mir2Lir::InsertCaseLabel(int vaddr, int keyVal) {
  SafeMap<unsigned int, LIR*>::iterator it;
  it = boundary_map_.find(vaddr);
  if (it == boundary_map_.end()) {
    LOG(FATAL) << "Error: didn't find vaddr 0x" << std::hex << vaddr;
  }
  LIR* new_label = static_cast<LIR*>(arena_->Alloc(sizeof(LIR), ArenaAllocator::kAllocLIR));
  new_label->dalvik_offset = vaddr;
  new_label->opcode = kPseudoCaseLabel;
  new_label->operands[0] = keyVal;
  InsertLIRAfter(it->second, new_label);
  return new_label;
}

void Mir2Lir::MarkPackedCaseLabels(Mir2Lir::SwitchTable *tab_rec) {
  const uint16_t* table = tab_rec->table;
  int base_vaddr = tab_rec->vaddr;
  const int *targets = reinterpret_cast<const int*>(&table[4]);
  int entries = table[1];
  int low_key = s4FromSwitchData(&table[2]);
  for (int i = 0; i < entries; i++) {
    tab_rec->targets[i] = InsertCaseLabel(base_vaddr + targets[i], i + low_key);
  }
}

void Mir2Lir::MarkSparseCaseLabels(Mir2Lir::SwitchTable *tab_rec) {
  const uint16_t* table = tab_rec->table;
  int base_vaddr = tab_rec->vaddr;
  int entries = table[1];
  const int* keys = reinterpret_cast<const int*>(&table[2]);
  const int* targets = &keys[entries];
  for (int i = 0; i < entries; i++) {
    tab_rec->targets[i] = InsertCaseLabel(base_vaddr + targets[i], keys[i]);
  }
}

void Mir2Lir::ProcessSwitchTables() {
  GrowableArray<SwitchTable*>::Iterator iterator(&switch_tables_);
  while (true) {
    Mir2Lir::SwitchTable *tab_rec = iterator.Next();
    if (tab_rec == NULL) break;
    if (tab_rec->table[0] == Instruction::kPackedSwitchSignature) {
      MarkPackedCaseLabels(tab_rec);
    } else if (tab_rec->table[0] == Instruction::kSparseSwitchSignature) {
      MarkSparseCaseLabels(tab_rec);
    } else {
      LOG(FATAL) << "Invalid switch table";
    }
  }
}

void Mir2Lir::DumpSparseSwitchTable(const uint16_t* table) {
  /*
   * Sparse switch data format:
   *  ushort ident = 0x0200   magic value
   *  ushort size       number of entries in the table; > 0
   *  int keys[size]      keys, sorted low-to-high; 32-bit aligned
   *  int targets[size]     branch targets, relative to switch opcode
   *
   * Total size is (2+size*4) 16-bit code units.
   */
  uint16_t ident = table[0];
  int entries = table[1];
  const int* keys = reinterpret_cast<const int*>(&table[2]);
  const int* targets = &keys[entries];
  LOG(INFO) <<  "Sparse switch table - ident:0x" << std::hex << ident
            << ", entries: " << std::dec << entries;
  for (int i = 0; i < entries; i++) {
    LOG(INFO) << "  Key[" << keys[i] << "] -> 0x" << std::hex << targets[i];
  }
}

void Mir2Lir::DumpPackedSwitchTable(const uint16_t* table) {
  /*
   * Packed switch data format:
   *  ushort ident = 0x0100   magic value
   *  ushort size       number of entries in the table
   *  int first_key       first (and lowest) switch case value
   *  int targets[size]     branch targets, relative to switch opcode
   *
   * Total size is (4+size*2) 16-bit code units.
   */
  uint16_t ident = table[0];
  const int* targets = reinterpret_cast<const int*>(&table[4]);
  int entries = table[1];
  int low_key = s4FromSwitchData(&table[2]);
  LOG(INFO) << "Packed switch table - ident:0x" << std::hex << ident
            << ", entries: " << std::dec << entries << ", low_key: " << low_key;
  for (int i = 0; i < entries; i++) {
    LOG(INFO) << "  Key[" << (i + low_key) << "] -> 0x" << std::hex
              << targets[i];
  }
}

/*
 * Set up special LIR to mark a Dalvik byte-code instruction start and
 * record it in the boundary_map.  NOTE: in cases such as kMirOpCheck in
 * which we split a single Dalvik instruction, only the first MIR op
 * associated with a Dalvik PC should be entered into the map.
 */
LIR* Mir2Lir::MarkBoundary(int offset, const char* inst_str) {
  LIR* res = NewLIR1(kPseudoDalvikByteCodeBoundary, reinterpret_cast<uintptr_t>(inst_str));
  if (boundary_map_.find(offset) == boundary_map_.end()) {
    boundary_map_.Put(offset, res);
  }
  return res;
}

bool Mir2Lir::EvaluateBranch(Instruction::Code opcode, int32_t src1, int32_t src2) {
  bool is_taken;
  switch (opcode) {
    case Instruction::IF_EQ: is_taken = (src1 == src2); break;
    case Instruction::IF_NE: is_taken = (src1 != src2); break;
    case Instruction::IF_LT: is_taken = (src1 < src2); break;
    case Instruction::IF_GE: is_taken = (src1 >= src2); break;
    case Instruction::IF_GT: is_taken = (src1 > src2); break;
    case Instruction::IF_LE: is_taken = (src1 <= src2); break;
    case Instruction::IF_EQZ: is_taken = (src1 == 0); break;
    case Instruction::IF_NEZ: is_taken = (src1 != 0); break;
    case Instruction::IF_LTZ: is_taken = (src1 < 0); break;
    case Instruction::IF_GEZ: is_taken = (src1 >= 0); break;
    case Instruction::IF_GTZ: is_taken = (src1 > 0); break;
    case Instruction::IF_LEZ: is_taken = (src1 <= 0); break;
    default:
      LOG(FATAL) << "Unexpected opcode " << opcode;
      is_taken = false;
  }
  return is_taken;
}

// Convert relation of src1/src2 to src2/src1
ConditionCode Mir2Lir::FlipComparisonOrder(ConditionCode before) {
  ConditionCode res;
  switch (before) {
    case kCondEq: res = kCondEq; break;
    case kCondNe: res = kCondNe; break;
    case kCondLt: res = kCondGt; break;
    case kCondGt: res = kCondLt; break;
    case kCondLe: res = kCondGe; break;
    case kCondGe: res = kCondLe; break;
    default:
      res = static_cast<ConditionCode>(0);
      LOG(FATAL) << "Unexpected ccode " << before;
  }
  return res;
}

// TODO: move to mir_to_lir.cc
Mir2Lir::Mir2Lir(CompilationUnit* cu, MIRGraph* mir_graph, ArenaAllocator* arena)
    : Backend(arena),
      literal_list_(NULL),
      method_literal_list_(NULL),
      code_literal_list_(NULL),
      cu_(cu),
      mir_graph_(mir_graph),
      switch_tables_(arena, 4, kGrowableArraySwitchTables),
      fill_array_data_(arena, 4, kGrowableArrayFillArrayData),
      throw_launchpads_(arena, 2048, kGrowableArrayThrowLaunchPads),
      suspend_launchpads_(arena, 4, kGrowableArraySuspendLaunchPads),
      intrinsic_launchpads_(arena, 2048, kGrowableArrayMisc),
      data_offset_(0),
      total_size_(0),
      block_label_list_(NULL),
      current_dalvik_offset_(0),
      reg_pool_(NULL),
      live_sreg_(0),
      num_core_spills_(0),
      num_fp_spills_(0),
      frame_size_(0),
      core_spill_mask_(0),
      fp_spill_mask_(0),
      first_lir_insn_(NULL),
      last_lir_insn_(NULL) {
  promotion_map_ = static_cast<PromotionMap*>
      (arena_->Alloc((cu_->num_dalvik_registers  + cu_->num_compiler_temps + 1) *
                      sizeof(promotion_map_[0]), ArenaAllocator::kAllocRegAlloc));
}

void Mir2Lir::Materialize() {
  CompilerInitializeRegAlloc();  // Needs to happen after SSA naming

  /* Allocate Registers using simple local allocation scheme */
  SimpleRegAlloc();

  if (mir_graph_->IsSpecialCase()) {
      /*
       * Custom codegen for special cases.  If for any reason the
       * special codegen doesn't succeed, first_lir_insn_ will
       * set to NULL;
       */
      SpecialMIR2LIR(mir_graph_->GetSpecialCase());
    }

  /* Convert MIR to LIR, etc. */
  if (first_lir_insn_ == NULL) {
    MethodMIR2LIR();
  }

  /* Method is not empty */
  if (first_lir_insn_) {
    // mark the targets of switch statement case labels
    ProcessSwitchTables();

    /* Convert LIR into machine code. */
    AssembleLIR();

    if (cu_->verbose) {
      CodegenDump();
    }
  }
}

CompiledMethod* Mir2Lir::GetCompiledMethod() {
  // Combine vmap tables - core regs, then fp regs - into vmap_table
  std::vector<uint16_t> raw_vmap_table;
  // Core regs may have been inserted out of order - sort first
  std::sort(core_vmap_table_.begin(), core_vmap_table_.end());
  for (size_t i = 0 ; i < core_vmap_table_.size(); ++i) {
    // Copy, stripping out the phys register sort key
    raw_vmap_table.push_back(~(-1 << VREG_NUM_WIDTH) & core_vmap_table_[i]);
  }
  // If we have a frame, push a marker to take place of lr
  if (frame_size_ > 0) {
    raw_vmap_table.push_back(INVALID_VREG);
  } else {
    DCHECK_EQ(__builtin_popcount(core_spill_mask_), 0);
    DCHECK_EQ(__builtin_popcount(fp_spill_mask_), 0);
  }
  // Combine vmap tables - core regs, then fp regs. fp regs already sorted
  for (uint32_t i = 0; i < fp_vmap_table_.size(); i++) {
    raw_vmap_table.push_back(fp_vmap_table_[i]);
  }
  UnsignedLeb128EncodingVector vmap_encoder;
  // Prefix the encoded data with its size.
  vmap_encoder.PushBack(raw_vmap_table.size());
  for (uint16_t cur : raw_vmap_table) {
    vmap_encoder.PushBack(cur);
  }
  CompiledMethod* result =
      new CompiledMethod(*cu_->compiler_driver, cu_->instruction_set, code_buffer_, frame_size_,
                         core_spill_mask_, fp_spill_mask_, encoded_mapping_table_.GetData(),
                         vmap_encoder.GetData(), native_gc_map_);
  return result;
}

int Mir2Lir::ComputeFrameSize() {
  /* Figure out the frame size */
  static const uint32_t kAlignMask = kStackAlignment - 1;
  uint32_t size = (num_core_spills_ + num_fp_spills_ +
                   1 /* filler word */ + cu_->num_regs + cu_->num_outs +
                   cu_->num_compiler_temps + 1 /* cur_method* */)
                   * sizeof(uint32_t);
  /* Align and set */
  return (size + kAlignMask) & ~(kAlignMask);
}

/*
 * Append an LIR instruction to the LIR list maintained by a compilation
 * unit
 */
void Mir2Lir::AppendLIR(LIR* lir) {
  if (first_lir_insn_ == NULL) {
    DCHECK(last_lir_insn_ == NULL);
    last_lir_insn_ = first_lir_insn_ = lir;
    lir->prev = lir->next = NULL;
  } else {
    last_lir_insn_->next = lir;
    lir->prev = last_lir_insn_;
    lir->next = NULL;
    last_lir_insn_ = lir;
  }
}

/*
 * Insert an LIR instruction before the current instruction, which cannot be the
 * first instruction.
 *
 * prev_lir <-> new_lir <-> current_lir
 */
void Mir2Lir::InsertLIRBefore(LIR* current_lir, LIR* new_lir) {
  DCHECK(current_lir->prev != NULL);
  LIR *prev_lir = current_lir->prev;

  prev_lir->next = new_lir;
  new_lir->prev = prev_lir;
  new_lir->next = current_lir;
  current_lir->prev = new_lir;
}

/*
 * Insert an LIR instruction after the current instruction, which cannot be the
 * first instruction.
 *
 * current_lir -> new_lir -> old_next
 */
void Mir2Lir::InsertLIRAfter(LIR* current_lir, LIR* new_lir) {
  new_lir->prev = current_lir;
  new_lir->next = current_lir->next;
  current_lir->next = new_lir;
  new_lir->next->prev = new_lir;
}


}  // namespace art
