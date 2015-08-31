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


/*! \file LowerHelper.cpp
    \brief This file implements helper functions for lowering

With NCG O0: all registers are hard-coded ;
With NCG O1: the lowering module will use variables that will be allocated to a physical register by the register allocator.

register types: FS 32-bit or 64-bit;
                XMM: SS(32-bit) SD (64-bit);
                GPR: 8-bit, 16-bit, 32-bit;
LowOpndRegType tells whether it is gpr, xmm or fs;
OpndSize can be OpndSize_8, OpndSize_16, OpndSize_32, OpndSize_64

A single native instruction can use multiple physical registers.
  we can't call freeReg in the middle of emitting a native instruction,
  since it may free the physical register used by an operand and cause two operands being allocated to the same physical register.

When allocating a physical register for an operand, we can't spill the operands that are already allocated. To avoid that, we call startNativeCode before each native instruction, here flag "canSpill" is set to true for each physical register;
  when a physical register is allocated, we set its flag "canSpill" to false;
  at end of each native instruction, call endNativeCode to set flag "canSpill" to true.
*/

#include "libdex/DexOpcodes.h"
#include "libdex/DexFile.h"
#include "Lower.h"
#include "NcgAot.h"
#include "enc_wrapper.h"
#include "vm/mterp/Mterp.h"
#include "vm/mterp/common/FindInterface.h"
#include "NcgHelper.h"
#include <math.h>
#include "interp/InterpState.h"

extern "C" int64_t __divdi3(int64_t, int64_t);
extern "C" int64_t __moddi3(int64_t, int64_t);
bool isScratchPhysical;
LowOp* lirTable[200];
int num_lirs_in_table = 0;

//4 tables are defined: GPR integer ALU ops, ALU ops in FPU, SSE 32-bit, SSE 64-bit
//the index to the table is the opcode
//add_opc,    or_opc,     adc_opc,    sbb_opc,
//and_opc,    sub_opc,    xor_opc,    cmp_opc,
//mul_opc,    imul_opc,   div_opc,    idiv_opc,
//sll_opc,    srl_opc,    sra, (SSE)
//shl_opc,    shr_opc,    sal_opc,    sar_opc, //integer shift
//neg_opc,    not_opc,    andn_opc, (SSE)
//n_alu
//!mnemonic for integer ALU operations
const  Mnemonic map_of_alu_opcode_2_mnemonic[] = {
    Mnemonic_ADD,  Mnemonic_OR,   Mnemonic_ADC,  Mnemonic_SBB,
    Mnemonic_AND,  Mnemonic_SUB,  Mnemonic_XOR,  Mnemonic_CMP,
    Mnemonic_MUL,  Mnemonic_IMUL, Mnemonic_DIV,  Mnemonic_IDIV,
    Mnemonic_Null, Mnemonic_Null, Mnemonic_Null,
    Mnemonic_SHL,  Mnemonic_SHR,  Mnemonic_SAL,  Mnemonic_SAR,
    Mnemonic_NEG,  Mnemonic_NOT,  Mnemonic_Null,
    Mnemonic_Null
};
//!mnemonic for ALU operations in FPU
const  Mnemonic map_of_fpu_opcode_2_mnemonic[] = {
    Mnemonic_FADD,  Mnemonic_Null,  Mnemonic_Null,  Mnemonic_Null,
    Mnemonic_Null,  Mnemonic_FSUB,  Mnemonic_Null,  Mnemonic_Null,
    Mnemonic_FMUL,  Mnemonic_Null,  Mnemonic_FDIV,  Mnemonic_Null,
    Mnemonic_Null,  Mnemonic_Null,
    Mnemonic_Null,  Mnemonic_Null,  Mnemonic_Null,  Mnemonic_Null,
    Mnemonic_Null,  Mnemonic_Null,  Mnemonic_Null,
    Mnemonic_Null
};
//!mnemonic for SSE 32-bit
const  Mnemonic map_of_sse_opcode_2_mnemonic[] = {
    Mnemonic_ADDSD,  Mnemonic_Null,  Mnemonic_Null,  Mnemonic_Null,
    Mnemonic_Null,   Mnemonic_SUBSD, Mnemonic_XORPD, Mnemonic_Null,
    Mnemonic_MULSD,  Mnemonic_Null,  Mnemonic_DIVSD,  Mnemonic_Null,
    Mnemonic_Null,   Mnemonic_Null,
    Mnemonic_Null,   Mnemonic_Null,  Mnemonic_Null,  Mnemonic_Null,
    Mnemonic_Null,   Mnemonic_Null,  Mnemonic_Null,
    Mnemonic_Null
};
//!mnemonic for SSE 64-bit integer
const  Mnemonic map_of_64_opcode_2_mnemonic[] = {
    Mnemonic_PADDQ, Mnemonic_POR,   Mnemonic_Null,  Mnemonic_Null,
    Mnemonic_PAND,  Mnemonic_PSUBQ, Mnemonic_PXOR,  Mnemonic_Null,
    Mnemonic_Null,  Mnemonic_Null,  Mnemonic_Null,  Mnemonic_Null,
    Mnemonic_PSLLQ, Mnemonic_PSRLQ, Mnemonic_Null,
    Mnemonic_Null,  Mnemonic_Null,  Mnemonic_Null,  Mnemonic_Null,
    Mnemonic_Null,  Mnemonic_Null,  Mnemonic_PANDN,
    Mnemonic_Null
};

////////////////////////////////////////////////
//!update fields of LowOpndReg

//!
void set_reg_opnd(LowOpndReg* op_reg, int reg, bool isPhysical, LowOpndRegType type) {
    op_reg->regType = type;
    if(isPhysical) {
        op_reg->logicalReg = -1;
        op_reg->physicalReg = reg;
    }
    else
        op_reg->logicalReg = reg;
    return;
}
//!update fields of LowOpndMem

//!
void set_mem_opnd(LowOpndMem* mem, int disp, int base, bool isPhysical) {
    mem->m_disp.value = disp;
    mem->hasScale = false;
    mem->m_base.regType = LowOpndRegType_gp;
    if(isPhysical) {
        mem->m_base.logicalReg = -1;
        mem->m_base.physicalReg = base;
    } else {
        mem->m_base.logicalReg = base;
    }
    return;
}
//!update fields of LowOpndMem

//!
void set_mem_opnd_scale(LowOpndMem* mem, int base, bool isPhysical, int disp, int index, bool indexPhysical, int scale) {
    mem->hasScale = true;
    mem->m_base.regType = LowOpndRegType_gp;
    if(isPhysical) {
        mem->m_base.logicalReg = -1;
        mem->m_base.physicalReg = base;
    } else {
        mem->m_base.logicalReg = base;
    }
    if(indexPhysical) {
        mem->m_index.logicalReg = -1;
        mem->m_index.physicalReg = index;
    } else {
        mem->m_index.logicalReg = index;
    }
    mem->m_disp.value = disp;
    mem->m_scale.value = scale;
    return;
}
//!return either LowOpndRegType_xmm or LowOpndRegType_gp

//!
inline LowOpndRegType getTypeFromIntSize(OpndSize size) {
    return size == OpndSize_64 ? LowOpndRegType_xmm : LowOpndRegType_gp;
}

// copied from JIT compiler
typedef struct AtomMemBlock {
    size_t bytesAllocated;
    struct AtomMemBlock *next;
    char ptr[0];
} AtomMemBlock;

#define ATOMBLOCK_DEFAULT_SIZE 4096
AtomMemBlock *atomMemHead = NULL;
AtomMemBlock *currentAtomMem = NULL;
void * atomNew(size_t size) {
    lowOpTimeStamp++; //one LowOp constructed
    if(atomMemHead == NULL) {
        atomMemHead = (AtomMemBlock*)malloc(sizeof(AtomMemBlock) + ATOMBLOCK_DEFAULT_SIZE);
        if(atomMemHead == NULL) {
            ALOGE("Memory allocation failed");
            return NULL;
        }
        currentAtomMem = atomMemHead;
        currentAtomMem->bytesAllocated = 0;
        currentAtomMem->next = NULL;
    }
    size = (size + 3) & ~3;
    if (size > ATOMBLOCK_DEFAULT_SIZE) {
        ALOGE("Requesting %d bytes which exceed the maximal size allowed", size);
        return NULL;
    }
retry:
    if (size + currentAtomMem->bytesAllocated <= ATOMBLOCK_DEFAULT_SIZE) {
        void *ptr;
        ptr = &currentAtomMem->ptr[currentAtomMem->bytesAllocated];
        return ptr;
    }
    if (currentAtomMem->next) {
        currentAtomMem = currentAtomMem->next;
        goto retry;
    }
    /* Time to allocate a new arena */
    AtomMemBlock *newAtomMem = (AtomMemBlock*)malloc(sizeof(AtomMemBlock) + ATOMBLOCK_DEFAULT_SIZE);
    if(newAtomMem == NULL) {
        ALOGE("Memory allocation failed");
        return NULL;
    }
    newAtomMem->bytesAllocated = 0;
    newAtomMem->next = NULL;
    currentAtomMem->next = newAtomMem;
    currentAtomMem = newAtomMem;
    goto retry;
    ALOGE("atomNew requesting %d bytes", size);
    return NULL;
}

void freeAtomMem() {
    //LOGI("free all atom memory");
    AtomMemBlock * tmpMem = atomMemHead;
    while(tmpMem != NULL) {
        tmpMem->bytesAllocated = 0;
        tmpMem = tmpMem->next;
    }
    currentAtomMem = atomMemHead;
}

LowOpImm* dump_special(AtomOpCode cc, int imm) {
    LowOpImm* op = (LowOpImm*)atomNew(sizeof(LowOpImm));
    op->lop.opCode = Mnemonic_NULL;
    op->lop.opCode2 = cc;
    op->lop.opnd1.type = LowOpndType_Imm;
    op->lop.numOperands = 1;
    op->immOpnd.value = imm;
    //stream = encoder_imm(m, size, imm, stream);
    return op;
}

LowOpLabel* lower_label(Mnemonic m, OpndSize size, int imm, const char* label, bool isLocal) {
    stream = encoder_imm(m, size, imm, stream);
    return NULL;
}

LowOpLabel* dump_label(Mnemonic m, OpndSize size, int imm,
               const char* label, bool isLocal) {
    return lower_label(m, size, imm, label, isLocal);
}

LowOpNCG* dump_ncg(Mnemonic m, OpndSize size, int imm) {
    stream = encoder_imm(m, size, imm, stream);
    return NULL;
}

//!update fields of LowOp and generate a x86 instruction with a single immediate operand

//!
LowOpImm* lower_imm(Mnemonic m, OpndSize size, int imm, bool updateTable) {
    stream = encoder_imm(m, size, imm, stream);
    return NULL;
}

LowOpImm* dump_imm(Mnemonic m, OpndSize size, int imm) {
    return lower_imm(m, size, imm, true);
}

LowOpImm* dump_imm_with_codeaddr(Mnemonic m, OpndSize size,
               int imm, char* codePtr) {
    encoder_imm(m, size, imm, codePtr);
    return NULL;
}

//!update fields of LowOp and generate a x86 instruction that takes a single memory operand

//!With NCG O1, we call freeReg to free up physical registers, then call registerAlloc to allocate a physical register for memory base
LowOpMem* lower_mem(Mnemonic m, AtomOpCode m2, OpndSize size,
               int disp, int base_reg) {
    stream = encoder_mem(m, size, disp, base_reg, true, stream);
    return NULL;
}

LowOpMem* dump_mem(Mnemonic m, AtomOpCode m2, OpndSize size,
               int disp, int base_reg, bool isBasePhysical) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        freeReg(true);
        //type of the base is gpr
        int regAll = registerAlloc(LowOpndRegType_gp, base_reg, isBasePhysical, true);
        return lower_mem(m, m2, size, disp, regAll);
    } else {
        stream = encoder_mem(m, size, disp, base_reg, isBasePhysical, stream);
        return NULL;
    }
}
//!update fields of LowOp and generate a x86 instruction that takes a single reg operand

//!With NCG O1, wecall freeReg to free up physical registers, then call registerAlloc to allocate a physical register for the single operand
LowOpReg* lower_reg(Mnemonic m, AtomOpCode m2, OpndSize size,
               int reg, LowOpndRegType type) {
    stream = encoder_reg(m, size, reg, true, type, stream);
    return NULL;
}

LowOpReg* dump_reg(Mnemonic m, AtomOpCode m2, OpndSize size,
               int reg, bool isPhysical, LowOpndRegType type) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        freeReg(true);
        if(m == Mnemonic_MUL || m == Mnemonic_IDIV) {
            //these two instructions use eax & edx implicitly
            touchEax();
            touchEdx();
        }
        int regAll = registerAlloc(type, reg, isPhysical, true);
        return lower_reg(m, m2, size, regAll, type);
    } else {
        stream = encoder_reg(m, size, reg, isPhysical, type, stream);
        return NULL;
    }
}
LowOpReg* dump_reg_noalloc(Mnemonic m, OpndSize size,
               int reg, bool isPhysical, LowOpndRegType type) {
    return lower_reg(m, ATOM_NORMAL, size, reg, type);
}

LowOpRegReg* lower_reg_reg(Mnemonic m, AtomOpCode m2, OpndSize size,
                 int reg, int reg2, LowOpndRegType type) {
    if(m == Mnemonic_FUCOMP || m == Mnemonic_FUCOM) {
        stream = encoder_compare_fp_stack(m == Mnemonic_FUCOMP,
                                          reg-reg2, size==OpndSize_64, stream);
    }
    else {
        stream = encoder_reg_reg(m, size, reg, true, reg2, true, type, stream);
    }
    return NULL;
}

//!update fields of LowOp and generate a x86 instruction that takes two reg operands

//Here, both registers are physical
LowOpRegReg* dump_reg_reg_noalloc(Mnemonic m, OpndSize size,
                           int reg, bool isPhysical,
                           int reg2, bool isPhysical2, LowOpndRegType type) {
    return lower_reg_reg(m, ATOM_NORMAL, size, reg, reg2, type);
}

inline bool isMnemonicMove(Mnemonic m) {
    return (m == Mnemonic_MOV || m == Mnemonic_MOVQ ||
            m == Mnemonic_MOVSS || m == Mnemonic_MOVSD);
}
//!update fields of LowOp and generate a x86 instruction that takes two reg operands

//!here dst reg is already allocated to a physical reg
//! we should not spill the physical register for dst when allocating for src
LowOpRegReg* dump_reg_reg_noalloc_dst(Mnemonic m, OpndSize size,
                               int reg, bool isPhysical,
                               int reg2, bool isPhysical2, LowOpndRegType type) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        int regAll = registerAlloc(type, reg, isPhysical, true);
        /* remove move from one register to the same register */
        if(isMnemonicMove(m) && regAll == reg2) return NULL;
        return lower_reg_reg(m, ATOM_NORMAL, size, regAll, reg2, type);
    } else {
        stream = encoder_reg_reg(m, size, reg, isPhysical, reg2, isPhysical2, type, stream);
        return NULL;
    }
}
//!update fields of LowOp and generate a x86 instruction that takes two reg operands

//!here src reg is already allocated to a physical reg
LowOpRegReg* dump_reg_reg_noalloc_src(Mnemonic m, AtomOpCode m2, OpndSize size,
                               int reg, bool isPhysical,
                               int reg2, bool isPhysical2, LowOpndRegType type) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        int regAll2;
        if(isMnemonicMove(m) && checkTempReg2(reg2, type, isPhysical2, reg)) { //dst reg is logical
            //only from get_virtual_reg_all
            regAll2 = registerAllocMove(reg2, type, isPhysical2, reg);
        } else {
            regAll2 = registerAlloc(type, reg2, isPhysical2, true);
            return lower_reg_reg(m, m2, size, reg, regAll2, type);
        }
    } else {
        stream = encoder_reg_reg(m, size, reg, isPhysical, reg2, isPhysical2, type, stream);
        return NULL;
    }
    return NULL;
}
//!update fields of LowOp and generate a x86 instruction that takes two reg operands

//!
LowOpRegReg* dump_reg_reg(Mnemonic m, AtomOpCode m2, OpndSize size,
                   int reg, bool isPhysical,
                   int reg2, bool isPhysical2, LowOpndRegType type) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        startNativeCode(-1, -1);
        //reg is source if m is MOV
        freeReg(true);
        int regAll = registerAlloc(type, reg, isPhysical, true);
        int regAll2;
        LowOpRegReg* op = NULL;
#ifdef MOVE_OPT2
        if(isMnemonicMove(m) &&
           ((reg != PhysicalReg_EDI && reg != PhysicalReg_ESP && reg != PhysicalReg_EBP) || (!isPhysical)) &&
           isPhysical2 == false) { //dst reg is logical
            //called from move_reg_to_reg
            regAll2 = registerAllocMove(reg2, type, isPhysical2, regAll);
        } else {
#endif
            donotSpillReg(regAll);
            regAll2 = registerAlloc(type, reg2, isPhysical2, true);
            op = lower_reg_reg(m, m2, size, regAll, regAll2, type);
#ifdef MOVE_OPT2
        }
#endif
        endNativeCode();
        return op;
    }
    else {
        stream = encoder_reg_reg(m, size, reg, isPhysical, reg2, isPhysical2, type, stream);
    }
    return NULL;
}

LowOpRegMem* lower_mem_reg(Mnemonic m, AtomOpCode m2, OpndSize size,
                 int disp, int base_reg,
                 MemoryAccessType mType, int mIndex,
                 int reg, LowOpndRegType type, bool isMoves) {
    if(m == Mnemonic_MOVSX) {
        stream = encoder_moves_mem_to_reg(size, disp, base_reg, true,
                                          reg, true, stream);
    }
    else if(m == Mnemonic_MOVZX) {
        stream = encoder_movez_mem_to_reg(size, disp, base_reg, true,
                                          reg, true, stream);
    }
    else {
        stream = encoder_mem_reg(m, size, disp, base_reg, true,
                                 reg, true, type, stream);
    }
    return NULL;
}

//!update fields of LowOp and generate a x86 instruction that takes one reg operand and one mem operand

//!Here, operands are already allocated to physical registers
LowOpRegMem* dump_mem_reg_noalloc(Mnemonic m, OpndSize size,
                           int disp, int base_reg, bool isBasePhysical,
                           MemoryAccessType mType, int mIndex,
                           int reg, bool isPhysical, LowOpndRegType type) {
    return lower_mem_reg(m, ATOM_NORMAL, size, disp, base_reg, mType, mIndex, reg, type, false);
}
//!update fields of LowOp and generate a x86 instruction that takes one reg operand and one mem operand

//!Here, memory operand is already allocated to physical register
LowOpRegMem* dump_mem_reg_noalloc_mem(Mnemonic m, AtomOpCode m2, OpndSize size,
                               int disp, int base_reg, bool isBasePhysical,
                               MemoryAccessType mType, int mIndex,
                               int reg, bool isPhysical, LowOpndRegType type) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        int regAll = registerAlloc(type, reg, isPhysical, true);
        return lower_mem_reg(m, m2, size, disp, base_reg, mType, mIndex, regAll, type, false);
    } else {
        stream = encoder_mem_reg(m, size, disp, base_reg, isBasePhysical,
                                 reg, isPhysical, type, stream);
    }
    return NULL;
}
//!update fields of LowOp and generate a x86 instruction that takes one reg operand and one mem operand

//!
LowOpRegMem* dump_mem_reg(Mnemonic m, AtomOpCode m2, OpndSize size,
                   int disp, int base_reg, bool isBasePhysical,
                   MemoryAccessType mType, int mIndex,
                   int reg, bool isPhysical, LowOpndRegType type) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        startNativeCode(-1, -1);
        freeReg(true);
        int baseAll = registerAlloc(LowOpndRegType_gp, base_reg, isBasePhysical, true);
        //it is okay to use the same physical register
        if(isMnemonicMove(m)) {
            freeReg(true);
        } else {
            donotSpillReg(baseAll);
        }
        int regAll = registerAlloc(type, reg, isPhysical, true);
        endNativeCode();
        return lower_mem_reg(m, m2, size, disp, baseAll, mType, mIndex, regAll, type, false);
    } else {
        stream = encoder_mem_reg(m, size, disp, base_reg, isBasePhysical,
                                 reg, isPhysical, type, stream);
    }
    return NULL;
}
//!update fields of LowOp and generate a x86 instruction that takes one reg operand and one mem operand

//!
LowOpRegMem* dump_moves_mem_reg(Mnemonic m, OpndSize size,
                         int disp, int base_reg, bool isBasePhysical,
             int reg, bool isPhysical) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        startNativeCode(-1, -1);
        freeReg(true);
        int baseAll = registerAlloc(LowOpndRegType_gp, base_reg, isBasePhysical, true);
        donotSpillReg(baseAll);
        int regAll = registerAlloc(LowOpndRegType_gp, reg, isPhysical, true);
        endNativeCode();
        return lower_mem_reg(m, ATOM_NORMAL, size, disp, baseAll, MemoryAccess_Unknown, -1,
            regAll, LowOpndRegType_gp, true/*moves*/);
    } else {
        stream = encoder_moves_mem_to_reg(size, disp, base_reg, isBasePhysical, reg, isPhysical, stream);
    }
    return NULL;
}
//!update fields of LowOp and generate a x86 instruction that takes one reg operand and one mem operand

//!
LowOpRegMem* dump_movez_mem_reg(Mnemonic m, OpndSize size,
             int disp, int base_reg, bool isBasePhysical,
             int reg, bool isPhysical) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        startNativeCode(-1, -1);
        freeReg(true);
        int baseAll = registerAlloc(LowOpndRegType_gp, base_reg, isBasePhysical, true);
        donotSpillReg(baseAll);
        int regAll = registerAlloc(LowOpndRegType_gp, reg, isPhysical, true);
        endNativeCode();
        return lower_mem_reg(m, ATOM_NORMAL, size, disp, baseAll, MemoryAccess_Unknown, -1,
            regAll, LowOpndRegType_gp, true/*moves*/);
    } else {
        stream = encoder_movez_mem_to_reg(size, disp, base_reg, isBasePhysical, reg, isPhysical, stream);
    }
    return NULL;
}

//!update fields of LowOp and generate a x86 instruction that takes one reg operand and one reg operand

//!
LowOpRegReg* dump_movez_reg_reg(Mnemonic m, OpndSize size,
             int reg, bool isPhysical,
             int reg2, bool isPhysical2) {
    LowOpRegReg* op = (LowOpRegReg*)atomNew(sizeof(LowOpRegReg));
    op->lop.opCode = m;
    op->lop.opnd1.size = OpndSize_32;
    op->lop.opnd1.type = LowOpndType_Reg;
    op->lop.opnd2.size = size;
    op->lop.opnd2.type = LowOpndType_Reg;
    set_reg_opnd(&(op->regOpnd1), reg2, isPhysical2, LowOpndRegType_gp);
    set_reg_opnd(&(op->regOpnd2), reg, isPhysical, LowOpndRegType_gp);
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        startNativeCode(-1, -1);
        //reg is source if m is MOV
        freeReg(true);
        int regAll = registerAlloc(LowOpndRegType_gp, reg, isPhysical, true);
        donotSpillReg(regAll);
        int regAll2 = registerAlloc(LowOpndRegType_gp, reg2, isPhysical2, true);
        stream = encoder_movez_reg_to_reg(size, regAll, true, regAll2, true,
                                          LowOpndRegType_gp, stream);
        endNativeCode();
    }
    else {
        stream = encoder_movez_reg_to_reg(size, reg, isPhysical, reg2,
                                        isPhysical2, LowOpndRegType_gp, stream);
    }
    return NULL;
}

//!update fields of LowOp and generate a x86 instruction that takes one reg operand and one mem operand

//!
LowOpRegMem* lower_mem_scale_reg(Mnemonic m, OpndSize size, int base_reg, int disp, int index_reg,
                 int scale, int reg, LowOpndRegType type) {
    bool isMovzs = (m == Mnemonic_MOVZX || m == Mnemonic_MOVSX);
    if(isMovzs)
        stream = encoder_movzs_mem_disp_scale_reg(m, size, base_reg, true, disp, index_reg, true,
                                                  scale, reg, true, type, stream);
    else {
        if(disp == 0)
            stream = encoder_mem_scale_reg(m, size, base_reg, true, index_reg, true,
                                           scale, reg, true, type, stream);
        else
            stream = encoder_mem_disp_scale_reg(m, size, base_reg, true, disp, index_reg, true,
                                                scale, reg, true, type, stream);
    }
    return NULL;
}

LowOpRegMem* dump_mem_scale_reg(Mnemonic m, OpndSize size,
                         int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale,
                         int reg, bool isPhysical, LowOpndRegType type) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        startNativeCode(-1, -1);
        freeReg(true);
        int baseAll = registerAlloc(LowOpndRegType_gp, base_reg, isBasePhysical, true);
        donotSpillReg(baseAll); //make sure index will not use the same physical reg
        int indexAll = registerAlloc(LowOpndRegType_gp, index_reg, isIndexPhysical, true);
        if(isMnemonicMove(m)) {
            freeReg(true);
            doSpillReg(baseAll); //base can be used now
        } else {
            donotSpillReg(indexAll);
        }
        bool isMovzs = (m == Mnemonic_MOVZX || m == Mnemonic_MOVSX);
        int regAll = registerAlloc(isMovzs ? LowOpndRegType_gp : type, reg, isPhysical, true);
        endNativeCode();
        return lower_mem_scale_reg(m, size, baseAll, disp, indexAll, scale, regAll, type);
    } else {
        stream = encoder_mem_scale_reg(m, size, base_reg, isBasePhysical, index_reg,
                                       isIndexPhysical, scale, reg, isPhysical, type, stream);
    }
    return NULL;
}
//!update fields of LowOp and generate a x86 instruction that takes one reg operand and one mem operand

//!
LowOpMemReg* lower_reg_mem_scale(Mnemonic m, OpndSize size, int reg,
                 int base_reg, int disp, int index_reg, int scale, LowOpndRegType type) {
    if(disp == 0)
        stream = encoder_reg_mem_scale(m, size, reg, true, base_reg, true,
                                       index_reg, true, scale, type, stream);
    else
        stream = encoder_reg_mem_disp_scale(m, size, reg, true, base_reg, true,
                                            disp, index_reg, true, scale, type, stream);
    return NULL;
}

LowOpMemReg* dump_reg_mem_scale(Mnemonic m, OpndSize size,
                         int reg, bool isPhysical,
                         int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale,
                         LowOpndRegType type) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        startNativeCode(-1, -1);
        freeReg(true);
        int baseAll = registerAlloc(LowOpndRegType_gp, base_reg, isBasePhysical, true);
        donotSpillReg(baseAll);
        int indexAll = registerAlloc(LowOpndRegType_gp, index_reg, isIndexPhysical, true);
        donotSpillReg(indexAll);
        int regAll = registerAlloc(type, reg, isPhysical, true);
        endNativeCode();
        return lower_reg_mem_scale(m, size, regAll, baseAll, disp, indexAll, scale, type);
    } else {
        stream = encoder_reg_mem_scale(m, size, reg, isPhysical, base_reg, isBasePhysical,
                                       index_reg, isIndexPhysical, scale, type, stream);
    }
    return NULL;
}
//!update fields of LowOp and generate a x86 instruction that takes one reg operand and one mem operand

//!Here operands are already allocated
LowOpMemReg* lower_reg_mem(Mnemonic m, AtomOpCode m2, OpndSize size, int reg,
                 int disp, int base_reg, MemoryAccessType mType, int mIndex,
                 LowOpndRegType type) {
    stream = encoder_reg_mem(m, size, reg, true, disp, base_reg, true, type, stream);
    return NULL;
}

LowOpMemReg* dump_reg_mem_noalloc(Mnemonic m, OpndSize size,
                           int reg, bool isPhysical,
                           int disp, int base_reg, bool isBasePhysical,
                           MemoryAccessType mType, int mIndex, LowOpndRegType type) {
    return lower_reg_mem(m, ATOM_NORMAL, size, reg, disp, base_reg, mType, mIndex, type);
}
//!update fields of LowOp and generate a x86 instruction that takes one reg operand and one mem operand

//!
LowOpMemReg* dump_reg_mem(Mnemonic m, AtomOpCode m2, OpndSize size,
                   int reg, bool isPhysical,
                   int disp, int base_reg, bool isBasePhysical,
                   MemoryAccessType mType, int mIndex, LowOpndRegType type) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        startNativeCode(-1, -1);
        freeReg(true);
        int baseAll = registerAlloc(LowOpndRegType_gp, base_reg, isBasePhysical, true);
        donotSpillReg(baseAll);
        int regAll = registerAlloc(type, reg, isPhysical, true);
        endNativeCode();
        return lower_reg_mem(m, m2, size, regAll, disp, baseAll, mType, mIndex, type);
    } else {
        stream = encoder_reg_mem(m, size, reg, isPhysical, disp, base_reg, isBasePhysical, type, stream);
    }
    return NULL;
}
//!update fields of LowOp and generate a x86 instruction that takes one immediate and one reg operand

//!The reg operand is allocated already
LowOpRegImm* lower_imm_reg(Mnemonic m, AtomOpCode m2, OpndSize size,
                 int imm, int reg, LowOpndRegType type, bool chaining) {
    stream = encoder_imm_reg(m, size, imm, reg, true, type, stream);
    return NULL;
}

LowOpRegImm* dump_imm_reg_noalloc(Mnemonic m, OpndSize size,
                           int imm, int reg, bool isPhysical, LowOpndRegType type) {
    return lower_imm_reg(m, ATOM_NORMAL, size, imm, reg, type, false);
}
//!update fields of LowOp and generate a x86 instruction that takes one immediate and one reg operand

//!
LowOpRegImm* dump_imm_reg(Mnemonic m, AtomOpCode m2, OpndSize size,
                   int imm, int reg, bool isPhysical, LowOpndRegType type, bool chaining) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        freeReg(true);
        int regAll = registerAlloc(type, reg, isPhysical, true);
        return lower_imm_reg(m, m2, size, imm, regAll, type, chaining);
    } else {
        stream = encoder_imm_reg(m, size, imm, reg, isPhysical, type, stream);
    }
    return NULL;
}
//!update fields of LowOp and generate a x86 instruction that takes one immediate and one mem operand

//!The mem operand is already allocated
LowOpMemImm* lower_imm_mem(Mnemonic m, AtomOpCode m2, OpndSize size, int imm,
                 int disp, int base_reg, MemoryAccessType mType, int mIndex,
                 bool chaining) {
    stream = encoder_imm_mem(m, size, imm, disp, base_reg, true, stream);
    return NULL;
}

LowOpMemImm* dump_imm_mem_noalloc(Mnemonic m, OpndSize size,
                           int imm,
                           int disp, int base_reg, bool isBasePhysical,
                           MemoryAccessType mType, int mIndex) {
    return lower_imm_mem(m, ATOM_NORMAL, size, imm, disp, base_reg, mType, mIndex, false);
}
//!update fields of LowOp and generate a x86 instruction that takes one immediate and one mem operand

//!
LowOpMemImm* dump_imm_mem(Mnemonic m, AtomOpCode m2, OpndSize size,
                   int imm,
                   int disp, int base_reg, bool isBasePhysical,
                   MemoryAccessType mType, int mIndex, bool chaining) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        /* do not free register if the base is %edi, %esp, or %ebp
           make sure dump_imm_mem will only generate a single instruction */
        if(!isBasePhysical || (base_reg != PhysicalReg_EDI &&
                               base_reg != PhysicalReg_ESP &&
                               base_reg != PhysicalReg_EBP)) {
            freeReg(true);
        }
        int baseAll = registerAlloc(LowOpndRegType_gp, base_reg, isBasePhysical, true);
        return lower_imm_mem(m, m2, size, imm, disp, baseAll, mType, mIndex, chaining);
    } else {
        stream = encoder_imm_mem(m, size, imm, disp, base_reg, isBasePhysical, stream);
    }
    return NULL;
}
//!update fields of LowOp and generate a x86 instruction that uses the FP stack and takes one mem operand

//!
LowOpMemReg* lower_fp_mem(Mnemonic m, OpndSize size, int reg,
                  int disp, int base_reg, MemoryAccessType mType, int mIndex) {
    stream = encoder_fp_mem(m, size, reg, disp, base_reg, true, stream);
    return NULL;
}

LowOpMemReg* dump_fp_mem(Mnemonic m, OpndSize size, int reg,
                  int disp, int base_reg, bool isBasePhysical,
                  MemoryAccessType mType, int mIndex) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        freeReg(true);
        int baseAll = registerAlloc(LowOpndRegType_gp, base_reg, isBasePhysical, true);
        return lower_fp_mem(m, size, reg, disp, baseAll, mType, mIndex);
    } else {
        stream = encoder_fp_mem(m, size, reg, disp, base_reg, isBasePhysical, stream);
    }
    return NULL;
}
//!update fields of LowOp and generate a x86 instruction that uses the FP stack and takes one mem operand

//!
LowOpRegMem* lower_mem_fp(Mnemonic m, OpndSize size, int disp, int base_reg,
                 MemoryAccessType mType, int mIndex, int reg) {
    stream = encoder_mem_fp(m, size, disp, base_reg, true, reg, stream);
    return NULL;
}

LowOpRegMem* dump_mem_fp(Mnemonic m, OpndSize size,
                  int disp, int base_reg, bool isBasePhysical,
                  MemoryAccessType mType, int mIndex,
                  int reg) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        freeReg(true);
        int baseAll = registerAlloc(LowOpndRegType_gp, base_reg, isBasePhysical, true);
        return lower_mem_fp(m, size, disp, baseAll, mType, mIndex, reg);
    } else {
        stream = encoder_mem_fp(m, size, disp, base_reg, isBasePhysical, reg, stream);
    }
    return NULL;
}
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
//OPERAND ORDER:
//LowOp same as EncoderBase destination first
//parameter order of function: src first

////////////////////////////////// IA32 native instructions //////////////
//! generate a native instruction lea

//!
void load_effective_addr(int disp, int base_reg, bool isBasePhysical,
                          int reg, bool isPhysical) {
    Mnemonic m = Mnemonic_LEA;
    dump_mem_reg(m, ATOM_NORMAL, OpndSize_32, disp, base_reg, isBasePhysical,
        MemoryAccess_Unknown, -1, reg, isPhysical, LowOpndRegType_gp);
}
//! generate a native instruction lea

//!
void load_effective_addr_scale(int base_reg, bool isBasePhysical,
                int index_reg, bool isIndexPhysical, int scale,
                int reg, bool isPhysical) {
    Mnemonic m = Mnemonic_LEA;
    dump_mem_scale_reg(m, OpndSize_32,
                              base_reg, isBasePhysical, 0/*disp*/, index_reg, isIndexPhysical, scale,
                              reg, isPhysical, LowOpndRegType_gp);
}
//!fldcw

//!
void load_fpu_cw(int disp, int base_reg, bool isBasePhysical) {
    Mnemonic m = Mnemonic_FLDCW;
    dump_mem(m, ATOM_NORMAL, OpndSize_16, disp, base_reg, isBasePhysical);
}
//!fnstcw

//!
void store_fpu_cw(bool checkException, int disp, int base_reg, bool isBasePhysical) {
    assert(!checkException);
    Mnemonic m = Mnemonic_FNSTCW;
    dump_mem(m, ATOM_NORMAL, OpndSize_16, disp, base_reg, isBasePhysical);
}
//!cdq

//!
void convert_integer(OpndSize srcSize, OpndSize dstSize) { //cbw, cwd, cdq
    assert(srcSize == OpndSize_32 && dstSize == OpndSize_64);
    Mnemonic m = Mnemonic_CDQ;
    dump_reg_reg(m, ATOM_NORMAL, OpndSize_32, PhysicalReg_EAX, true, PhysicalReg_EDX, true, LowOpndRegType_gp);
}
//!fld: load from memory (float or double) to stack

//!
void load_fp_stack(LowOp* op, OpndSize size, int disp, int base_reg, bool isBasePhysical) {//fld(s|l)
    Mnemonic m = Mnemonic_FLD;
    dump_mem_fp(m, size, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, 0); //ST0
}
//! fild: load from memory (int or long) to stack

//!
void load_int_fp_stack(OpndSize size, int disp, int base_reg, bool isBasePhysical) {//fild(ll|l)
    Mnemonic m = Mnemonic_FILD;
    dump_mem_fp(m, size, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, 0); //ST0
}
//!fild: load from memory (absolute addr)

//!
void load_int_fp_stack_imm(OpndSize size, int imm) {//fild(ll|l)
    return load_int_fp_stack(size, imm, PhysicalReg_Null, true);
}
//!fst: store from stack to memory (float or double)

//!
void store_fp_stack(LowOp* op, bool pop, OpndSize size, int disp, int base_reg, bool isBasePhysical) {//fst(p)(s|l)
    Mnemonic m = pop ? Mnemonic_FSTP : Mnemonic_FST;
    dump_fp_mem(m, size, 0, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1);
}
//!fist: store from stack to memory (int or long)

//!
void store_int_fp_stack(LowOp* op, bool pop, OpndSize size, int disp, int base_reg, bool isBasePhysical) {//fist(p)(l)
    Mnemonic m = pop ? Mnemonic_FISTP : Mnemonic_FIST;
    dump_fp_mem(m, size, 0, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1);
}
//!cmp reg, mem

//!
void compare_reg_mem(LowOp* op, OpndSize size, int reg, bool isPhysical,
              int disp, int base_reg, bool isBasePhysical) {
    Mnemonic m = Mnemonic_CMP;
    dump_reg_mem(m, ATOM_NORMAL, size, reg, isPhysical, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, getTypeFromIntSize(size));
}
//!cmp mem, reg

//!
void compare_mem_reg(OpndSize size,
              int disp, int base_reg, bool isBasePhysical,
              int reg, bool isPhysical) {
    Mnemonic m = Mnemonic_CMP;
    dump_mem_reg(m, ATOM_NORMAL, size, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, reg, isPhysical, getTypeFromIntSize(size));
}
//! compare a VR with a temporary variable

//!
void compare_VR_reg_all(OpndSize size,
             int vA,
             int reg, bool isPhysical, Mnemonic m) {
    LowOpndRegType type = getTypeFromIntSize(size);
    LowOpndRegType pType = type;
    if(m == Mnemonic_COMISS) {
        size = OpndSize_32;
        type = LowOpndRegType_ss;
        pType = LowOpndRegType_xmm;
    }
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        int tmpValue[2];
        int isConst = isVirtualRegConstant(vA, type, tmpValue, true/*updateRefCount*/);
        if(isConst == 3) {
            if(m == Mnemonic_COMISS) {
#ifdef DEBUG_NCG_O1
                LOGI("VR is const and SS in compare_VR_reg");
#endif
                dumpImmToMem(vA, OpndSize_32, tmpValue[0]);
                //dumpImmToMem(vA+1, OpndSize_32, 0); //CHECK necessary? will overwrite vA+1!!!
                dump_mem_reg(m, ATOM_NORMAL, size, 4*vA, PhysicalReg_FP, true, MemoryAccess_VR, vA, reg, isPhysical, pType);
                return;
            }
            else if(size != OpndSize_64) {
#ifdef DEBUG_NCG_O1
                LOGI("VR is const and 32 bits in compare_VR_reg");
#endif
                dump_imm_reg(m, ATOM_NORMAL, size, tmpValue[0], reg, isPhysical, pType, false);
                return;
            }
            else if(size == OpndSize_64) {
#ifdef DEBUG_NCG_O1
                LOGI("VR is const and 64 bits in compare_VR_reg");
#endif
                dumpImmToMem(vA, OpndSize_32, tmpValue[0]);
                dumpImmToMem(vA+1, OpndSize_32, tmpValue[1]);
                dump_mem_reg(m, ATOM_NORMAL, size, 4*vA, PhysicalReg_FP, true,
                    MemoryAccess_VR, vA, reg, isPhysical, pType);
                return;
            }
        }
        if(isConst == 1) dumpImmToMem(vA, OpndSize_32, tmpValue[0]);
        if(isConst == 2) dumpImmToMem(vA+1, OpndSize_32, tmpValue[1]);
        freeReg(true);
        int regAll = checkVirtualReg(vA, type, 0/*do not update*/);
        if(regAll != PhysicalReg_Null) { //do not spill regAll when allocating register for dst
            startNativeCode(-1, -1);
            donotSpillReg(regAll);
            dump_reg_reg_noalloc_src(m, ATOM_NORMAL, size, regAll, true, reg, isPhysical, pType);
            endNativeCode();
        }
        else {
            //virtual register is not allocated to a physical register
            dump_mem_reg_noalloc_mem(m, ATOM_NORMAL, size, 4*vA, PhysicalReg_FP, true,
                MemoryAccess_VR, vA, reg, isPhysical, pType);
        }
        updateRefCount(vA, type);
        return;
    } else {
        dump_mem_reg(m, ATOM_NORMAL, size, 4*vA, PhysicalReg_FP, true,
            MemoryAccess_VR, vA, reg, isPhysical, pType);
        return;
    }
}
void compare_VR_reg(OpndSize size,
             int vA,
             int reg, bool isPhysical) {
    Mnemonic m = Mnemonic_CMP;
    return compare_VR_reg_all(size, vA, reg, isPhysical, m);
}
void compare_VR_ss_reg(int vA, int reg, bool isPhysical) {
    Mnemonic m = Mnemonic_COMISS;
    return compare_VR_reg_all(OpndSize_32, vA, reg, isPhysical, m);
}
void compare_VR_sd_reg(int vA, int reg, bool isPhysical) {
    Mnemonic m = Mnemonic_COMISD;
    return compare_VR_reg_all(OpndSize_64, vA, reg, isPhysical, m);
}
//!load VR to stack

//!
void load_fp_stack_VR_all(OpndSize size, int vB, Mnemonic m) {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        //can't load from immediate to fp stack
        int tmpValue[2];
        int isConst = isVirtualRegConstant(vB, getTypeFromIntSize(size), tmpValue, false/*updateRefCount*/);
        if(isConst > 0) {
            if(size != OpndSize_64) {
#ifdef DEBUG_NCG_O1
                LOGI("VR is const and 32 bits in load_fp_stack");
#endif
                dumpImmToMem(vB, OpndSize_32, tmpValue[0]);
            }
            else {
#ifdef DEBUG_NCG_O1
                LOGI("VR is const and 64 bits in load_fp_stack_VR");
#endif
                if(isConst == 1 || isConst == 3) dumpImmToMem(vB, OpndSize_32, tmpValue[0]);
                if(isConst == 2 || isConst == 3) dumpImmToMem(vB+1, OpndSize_32, tmpValue[1]);
            }
        }
        else { //if VR was updated by a def of gp, a xfer point was inserted
            //if VR was updated by a def of xmm, a xfer point was inserted
#if 0
            int regAll = checkVirtualReg(vB, size, 1);
            if(regAll != PhysicalReg_Null) //dump from register to memory
                dump_reg_mem_noalloc(m, size, regAll, true, 4*vB, PhysicalReg_FP, true,
                    MemoryAccess_VR, vB, getTypeFromIntSize(size));
#endif
        }
        dump_mem_fp(m, size, 4*vB, PhysicalReg_FP, true, MemoryAccess_VR, vB, 0);
    } else {
        dump_mem_fp(m, size, 4*vB, PhysicalReg_FP, true, MemoryAccess_VR, vB, 0);
    }
}
//!load VR(float or double) to stack

//!
void load_fp_stack_VR(OpndSize size, int vA) {//fld(s|l)
    Mnemonic m = Mnemonic_FLD;
    return load_fp_stack_VR_all(size, vA, m);
}
//!load VR(int or long) to stack

//!
void load_int_fp_stack_VR(OpndSize size, int vA) {//fild(ll|l)
    Mnemonic m = Mnemonic_FILD;
    return load_fp_stack_VR_all(size, vA, m);
}
//!store from stack to VR (float or double)

//!
void store_fp_stack_VR(bool pop, OpndSize size, int vA) {//fst(p)(s|l)
    Mnemonic m = pop ? Mnemonic_FSTP : Mnemonic_FST;
    dump_fp_mem(m, size, 0, 4*vA, PhysicalReg_FP, true, MemoryAccess_VR, vA);
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        if(size == OpndSize_32)
            updateVirtualReg(vA, LowOpndRegType_fs_s);
        else
            updateVirtualReg(vA, LowOpndRegType_fs);
    }
}
//!store from stack to VR (int or long)

//!
void store_int_fp_stack_VR(bool pop, OpndSize size, int vA) {//fist(p)(l)
    Mnemonic m = pop ? Mnemonic_FISTP : Mnemonic_FIST;
    dump_fp_mem(m, size, 0, 4*vA, PhysicalReg_FP, true, MemoryAccess_VR, vA);
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        if(size == OpndSize_32)
            updateVirtualReg(vA, LowOpndRegType_fs_s);
        else
            updateVirtualReg(vA, LowOpndRegType_fs);
    }
}
//! ALU ops in FPU, one operand is a VR

//!
void fpu_VR(ALU_Opcode opc, OpndSize size, int vA) {
    Mnemonic m = map_of_fpu_opcode_2_mnemonic[opc];
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        int tmpValue[2];
        int isConst = isVirtualRegConstant(vA, getTypeFromIntSize(size), tmpValue, false/*updateRefCount*/);
        if(isConst > 0) {
            if(size != OpndSize_64) {
                //allocate a register for dst
                dumpImmToMem(vA, OpndSize_32, tmpValue[0]);
            }
            else {
                if((isConst == 1 || isConst == 3) && size == OpndSize_64) {
                    dumpImmToMem(vA, OpndSize_32, tmpValue[0]);
                }
                if((isConst == 2 || isConst == 3) && size == OpndSize_64) {
                    dumpImmToMem(vA+1, OpndSize_32, tmpValue[1]);
                }
            }
        }
        if(!isInMemory(vA, size)) {
            ALOGE("fpu_VR");
        }
        dump_mem_fp(m, size, 4*vA, PhysicalReg_FP, true, MemoryAccess_VR, vA, 0);
    } else {
        dump_mem_fp(m, size, 4*vA, PhysicalReg_FP, true, MemoryAccess_VR, vA, 0);
    }
}
//! cmp imm reg

//!
void compare_imm_reg(OpndSize size, int imm,
              int reg, bool isPhysical) {
    if(imm == 0) {
        LowOpndRegType type = getTypeFromIntSize(size);
        Mnemonic m = Mnemonic_TEST;
        if(gDvm.executionMode == kExecutionModeNcgO1) {
            freeReg(true);
            int regAll = registerAlloc(type, reg, isPhysical, true);
            lower_reg_reg(m, ATOM_NORMAL, size, regAll, regAll, type);
        } else {
            stream = encoder_reg_reg(m, size, reg, isPhysical, reg, isPhysical, type, stream);
        }
        return;
    }
    Mnemonic m = Mnemonic_CMP;
    dump_imm_reg(m, ATOM_NORMAL, size, imm, reg, isPhysical, getTypeFromIntSize(size), false);
}
//! cmp imm mem

//!
void compare_imm_mem(OpndSize size, int imm,
              int disp, int base_reg, bool isBasePhysical) {
    Mnemonic m = Mnemonic_CMP;
    dump_imm_mem(m, ATOM_NORMAL, size, imm, disp,
                        base_reg, isBasePhysical, MemoryAccess_Unknown, -1, false);
}
//! cmp imm VR

//!
void compare_imm_VR(OpndSize size, int imm,
             int vA) {
    Mnemonic m = Mnemonic_CMP;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        if(size != OpndSize_32) ALOGE("only 32 bits supported in compare_imm_VR");
        int tmpValue[2];
        int isConst = isVirtualRegConstant(vA, getTypeFromIntSize(size), tmpValue, false/*updateRefCount*/);
        if(isConst > 0) {
            dumpImmToMem(vA, OpndSize_32, tmpValue[0]);
        }
        int regAll = checkVirtualReg(vA, getTypeFromIntSize(size), 0);
        if(regAll != PhysicalReg_Null)
            dump_imm_reg_noalloc(m, size, imm, regAll, true, LowOpndRegType_gp);
        else
            dump_imm_mem_noalloc(m, size, imm, 4*vA, PhysicalReg_FP, true,
                MemoryAccess_VR, vA);
        updateRefCount(vA, getTypeFromIntSize(size));
    } else {
        dump_imm_mem(m, ATOM_NORMAL, size, imm, 4*vA, PhysicalReg_FP, true, MemoryAccess_VR, vA, false);
    }
}
//! cmp reg reg

//!
void compare_reg_reg(int reg1, bool isPhysical1,
              int reg2, bool isPhysical2) {
    Mnemonic m = Mnemonic_CMP;
    dump_reg_reg(m, ATOM_NORMAL, OpndSize_32, reg1, isPhysical1, reg2, isPhysical2, LowOpndRegType_gp);
}
void compare_reg_reg_16(int reg1, bool isPhysical1,
              int reg2, bool isPhysical2) {
    Mnemonic m = Mnemonic_CMP;
    dump_reg_reg(m, ATOM_NORMAL, OpndSize_16, reg1, isPhysical1, reg2, isPhysical2, LowOpndRegType_gp);
}

//! comiss mem reg

//!SSE, XMM: comparison of floating point numbers
void compare_ss_mem_reg(LowOp* op, int disp, int base_reg, bool isBasePhysical,
             int reg, bool isPhysical) {
    Mnemonic m = Mnemonic_COMISS;
    dump_mem_reg(m, ATOM_NORMAL, OpndSize_32, disp, base_reg, isBasePhysical,
        MemoryAccess_Unknown, -1, reg, isPhysical, LowOpndRegType_xmm);
}
//! comiss reg reg

//!
void compare_ss_reg_with_reg(LowOp* op, int reg1, bool isPhysical1,
                  int reg2, bool isPhysical2) {
    Mnemonic m = Mnemonic_COMISS;
    dump_reg_reg(m,  ATOM_NORMAL, OpndSize_32, reg1, isPhysical1, reg2, isPhysical2, LowOpndRegType_xmm);
}
//! comisd mem reg

//!
void compare_sd_mem_with_reg(LowOp* op, int disp, int base_reg, bool isBasePhysical,
                  int reg, bool isPhysical) {
    Mnemonic m = Mnemonic_COMISD;
    dump_mem_reg(m, ATOM_NORMAL, OpndSize_64, disp, base_reg, isBasePhysical,
        MemoryAccess_Unknown, -1, reg, isPhysical, LowOpndRegType_xmm);
}
//! comisd reg reg

//!
void compare_sd_reg_with_reg(LowOp* op, int reg1, bool isPhysical1,
                  int reg2, bool isPhysical2) {
    Mnemonic m = Mnemonic_COMISD;
    dump_reg_reg(m, ATOM_NORMAL, OpndSize_64, reg1, isPhysical1, reg2, isPhysical2, LowOpndRegType_xmm);
}
//! fucom[p]

//!
void compare_fp_stack(bool pop, int reg, bool isDouble) { //compare ST(0) with ST(reg)
    Mnemonic m = pop ? Mnemonic_FUCOMP : Mnemonic_FUCOM;
    lower_reg_reg(m, ATOM_NORMAL, isDouble ? OpndSize_64 : OpndSize_32,
                  PhysicalReg_ST0+reg, PhysicalReg_ST0, LowOpndRegType_fs);
}
/*!
\brief generate a single return instruction

*/
LowOp* lower_return() {
    stream = encoder_return(stream);
    return NULL;
}

void x86_return() {
    lower_return();
}

//!test imm reg

//!
void test_imm_reg(OpndSize size, int imm, int reg, bool isPhysical) {
    dump_imm_reg(Mnemonic_TEST, ATOM_NORMAL, size, imm, reg, isPhysical, getTypeFromIntSize(size), false);
}
//!test imm mem

//!
void test_imm_mem(OpndSize size, int imm, int disp, int reg, bool isPhysical) {
    dump_imm_mem(Mnemonic_TEST, ATOM_NORMAL, size, imm, disp, reg, isPhysical, MemoryAccess_Unknown, -1, false);
}
//!alu unary op with one reg operand

//!
void alu_unary_reg(OpndSize size, ALU_Opcode opc, int reg, bool isPhysical) {
    Mnemonic m;
    if(size == OpndSize_64)
        m = map_of_64_opcode_2_mnemonic[opc];
    else
        m = map_of_alu_opcode_2_mnemonic[opc];
    dump_reg(m, ATOM_NORMAL_ALU, size, reg, isPhysical, getTypeFromIntSize(size));
}
//!alu unary op with one mem operand

//!
void alu_unary_mem(LowOp* op, OpndSize size, ALU_Opcode opc, int disp, int base_reg, bool isBasePhysical) {
    Mnemonic m;
    if(size == OpndSize_64)
        m = map_of_64_opcode_2_mnemonic[opc];
    else
        m = map_of_alu_opcode_2_mnemonic[opc];
    dump_mem(m, ATOM_NORMAL_ALU, size, disp, base_reg, isBasePhysical);
}
//!alu binary op with immediate and one mem operand

//!
void alu_binary_imm_mem(OpndSize size, ALU_Opcode opc, int imm, int disp, int base_reg, bool isBasePhysical) {
    Mnemonic m;
    if(size == OpndSize_64)
        m = map_of_64_opcode_2_mnemonic[opc];
    else
        m = map_of_alu_opcode_2_mnemonic[opc];
    dump_imm_mem(m, ATOM_NORMAL_ALU, size, imm, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, false);
}
//!alu binary op with immediate and one reg operand

//!
void alu_binary_imm_reg(OpndSize size, ALU_Opcode opc, int imm, int reg, bool isPhysical) {
    Mnemonic m;
    if(size == OpndSize_64)
        m = map_of_64_opcode_2_mnemonic[opc];
    else
        m = map_of_alu_opcode_2_mnemonic[opc];
    dump_imm_reg(m, ATOM_NORMAL_ALU, size, imm, reg, isPhysical, getTypeFromIntSize(size), false);
}
//!alu binary op with one mem operand and one reg operand

//!
void alu_binary_mem_reg(OpndSize size, ALU_Opcode opc,
             int disp, int base_reg, bool isBasePhysical,
             int reg, bool isPhysical) {
    Mnemonic m;
    if(size == OpndSize_64)
        m = map_of_64_opcode_2_mnemonic[opc];
    else
        m = map_of_alu_opcode_2_mnemonic[opc];
    dump_mem_reg(m, ATOM_NORMAL_ALU, size, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, reg, isPhysical, getTypeFromIntSize(size));
}

void alu_sd_binary_VR_reg(ALU_Opcode opc, int vA, int reg, bool isPhysical, bool isSD) {
    Mnemonic m;
    if(isSD) m = map_of_sse_opcode_2_mnemonic[opc];
    else m = (Mnemonic)(map_of_sse_opcode_2_mnemonic[opc]+1); //from SD to SS
    OpndSize size = isSD ? OpndSize_64 : OpndSize_32;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        LowOpndRegType type = isSD ? LowOpndRegType_xmm : LowOpndRegType_ss; //type of the mem operand
        int tmpValue[2];
        int isConst = isVirtualRegConstant(vA, type, tmpValue,
                          true/*updateRefCount*/);
        if(isConst == 3 && !isSD) {
            //isConst can be 0 or 3, mem32, use xmm
            dumpImmToMem(vA, OpndSize_32, tmpValue[0]);
            dump_mem_reg(m, ATOM_NORMAL_ALU, OpndSize_32, 4*vA, PhysicalReg_FP, true,
                       MemoryAccess_VR, vA, reg, isPhysical,
                       LowOpndRegType_xmm);
            return;
        }
        if(isConst == 3 && isSD) {
            dumpImmToMem(vA, OpndSize_32, tmpValue[0]);
            dumpImmToMem(vA+1, OpndSize_32, tmpValue[1]);
            dump_mem_reg(m, ATOM_NORMAL_ALU, OpndSize_64, 4*vA, PhysicalReg_FP, true,
                       MemoryAccess_VR, vA, reg, isPhysical, LowOpndRegType_xmm);
            return;
        }
        if(isConst == 1) dumpImmToMem(vA, OpndSize_32, tmpValue[0]);
        if(isConst == 2) dumpImmToMem(vA+1, OpndSize_32, tmpValue[1]);
        freeReg(true);

        int regAll = checkVirtualReg(vA, type, 0/*do not update refCount*/);
        if(regAll != PhysicalReg_Null) {
            startNativeCode(-1, -1); //should we use vA, type
            //CHECK: callupdateVRAtUse
            donotSpillReg(regAll);
            dump_reg_reg_noalloc_src(m, ATOM_NORMAL_ALU, size, regAll, true, reg,
                         isPhysical, LowOpndRegType_xmm);
            endNativeCode();
        }
        else {
            dump_mem_reg_noalloc_mem(m, ATOM_NORMAL_ALU, size, 4*vA, PhysicalReg_FP, true,
                         MemoryAccess_VR, vA, reg, isPhysical, LowOpndRegType_xmm);
        }
        updateRefCount(vA, type);
    }
    else {
        dump_mem_reg(m, ATOM_NORMAL, size, 4*vA, PhysicalReg_FP, true,
                    MemoryAccess_VR, vA, reg, isPhysical, LowOpndRegType_xmm);
    }
}

//!alu binary op with a VR and one reg operand

//!
void alu_binary_VR_reg(OpndSize size, ALU_Opcode opc, int vA, int reg, bool isPhysical) {
    Mnemonic m;
    if(size == OpndSize_64)
        m = map_of_64_opcode_2_mnemonic[opc];
    else
        m = map_of_alu_opcode_2_mnemonic[opc];
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        int tmpValue[2];
        int isConst = isVirtualRegConstant(vA, getTypeFromIntSize(size), tmpValue,
                          true/*updateRefCount*/);
        if(isConst == 3 && size != OpndSize_64) {
            //allocate a register for dst
            dump_imm_reg(m, ATOM_NORMAL_ALU, size, tmpValue[0], reg, isPhysical,
                       getTypeFromIntSize(size), false);
            return;
        }
        if(isConst == 3 && size == OpndSize_64) {
            dumpImmToMem(vA, OpndSize_32, tmpValue[0]);
            dumpImmToMem(vA+1, OpndSize_32, tmpValue[1]);
            dump_mem_reg(m, ATOM_NORMAL_ALU, size, 4*vA, PhysicalReg_FP, true,
                MemoryAccess_VR, vA, reg, isPhysical, getTypeFromIntSize(size));
            return;
        }
        if(isConst == 1) dumpImmToMem(vA, OpndSize_32, tmpValue[0]);
        if(isConst == 2) dumpImmToMem(vA+1, OpndSize_32, tmpValue[1]);

        freeReg(true);
        int regAll = checkVirtualReg(vA, getTypeFromIntSize(size), 0);
        if(regAll != PhysicalReg_Null) {
            startNativeCode(-1, -1);
            donotSpillReg(regAll);
            dump_reg_reg_noalloc_src(m, ATOM_NORMAL_ALU, size, regAll, true, reg,
                         isPhysical, getTypeFromIntSize(size));
            endNativeCode();
        }
        else {
            dump_mem_reg_noalloc_mem(m, ATOM_NORMAL_ALU, size, 4*vA, PhysicalReg_FP, true,
                MemoryAccess_VR, vA, reg, isPhysical, getTypeFromIntSize(size));
        }
        updateRefCount(vA, getTypeFromIntSize(size));
    }
    else {
        dump_mem_reg(m, ATOM_NORMAL, size, 4*vA, PhysicalReg_FP, true,
            MemoryAccess_VR, vA, reg, isPhysical, getTypeFromIntSize(size));
    }
}
//!alu binary op with two reg operands

//!
void alu_binary_reg_reg(OpndSize size, ALU_Opcode opc,
                         int reg1, bool isPhysical1,
                         int reg2, bool isPhysical2) {
    Mnemonic m;
    if(size == OpndSize_64)
        m = map_of_64_opcode_2_mnemonic[opc];
    else
        m = map_of_alu_opcode_2_mnemonic[opc];
    dump_reg_reg(m, ATOM_NORMAL_ALU, size, reg1, isPhysical1, reg2, isPhysical2, getTypeFromIntSize(size));
}
//!alu binary op with one reg operand and one mem operand

//!
void alu_binary_reg_mem(OpndSize size, ALU_Opcode opc,
             int reg, bool isPhysical,
             int disp, int base_reg, bool isBasePhysical) { //destination is mem!!
    Mnemonic m;
    if(size == OpndSize_64)
        m = map_of_64_opcode_2_mnemonic[opc];
    else
        m = map_of_alu_opcode_2_mnemonic[opc];
    dump_reg_mem(m, ATOM_NORMAL_ALU, size, reg, isPhysical, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, getTypeFromIntSize(size));
}
//!FPU ops with one mem operand

//!
void fpu_mem(LowOp* op, ALU_Opcode opc, OpndSize size, int disp, int base_reg, bool isBasePhysical) {
    Mnemonic m = map_of_fpu_opcode_2_mnemonic[opc];
    dump_mem_fp(m, size, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, 0);
}
//!SSE 32-bit ALU

//!
void alu_ss_binary_reg_reg(ALU_Opcode opc, int reg, bool isPhysical,
                int reg2, bool isPhysical2) {
    Mnemonic m = (Mnemonic)(map_of_sse_opcode_2_mnemonic[opc]+1); //from SD to SS
    dump_reg_reg(m, ATOM_NORMAL_ALU, OpndSize_32, reg, isPhysical, reg2, isPhysical2, LowOpndRegType_xmm);
}
//!SSE 64-bit ALU

//!
void alu_sd_binary_reg_reg(ALU_Opcode opc, int reg, bool isPhysical,
                int reg2, bool isPhysical2) {
    Mnemonic m = map_of_sse_opcode_2_mnemonic[opc];
    dump_reg_reg(m, ATOM_NORMAL_ALU, OpndSize_64, reg, isPhysical, reg2, isPhysical2, LowOpndRegType_xmm);
}
//!push reg to native stack

//!
void push_reg_to_stack(OpndSize size, int reg, bool isPhysical) {
    dump_reg(Mnemonic_PUSH, ATOM_NORMAL, size, reg, isPhysical, getTypeFromIntSize(size));
}
//!push mem to native stack

//!
void push_mem_to_stack(OpndSize size, int disp, int base_reg, bool isBasePhysical) {
    dump_mem(Mnemonic_PUSH, ATOM_NORMAL, size, disp, base_reg, isBasePhysical);
}
//!move from reg to memory

//!
void move_reg_to_mem(OpndSize size,
                      int reg, bool isPhysical,
                      int disp, int base_reg, bool isBasePhysical) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    dump_reg_mem(m, ATOM_NORMAL, size, reg, isPhysical, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, getTypeFromIntSize(size));
}
//!move from reg to memory

//!Operands are already allocated
void move_reg_to_mem_noalloc(OpndSize size,
                  int reg, bool isPhysical,
                  int disp, int base_reg, bool isBasePhysical,
                  MemoryAccessType mType, int mIndex) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    dump_reg_mem_noalloc(m, size, reg, isPhysical, disp, base_reg, isBasePhysical, mType, mIndex, getTypeFromIntSize(size));
}
//!move from memory to reg

//!
LowOpRegMem* move_mem_to_reg(OpndSize size,
                      int disp, int base_reg, bool isBasePhysical,
                      int reg, bool isPhysical) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    return dump_mem_reg(m, ATOM_NORMAL, size, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, reg, isPhysical, getTypeFromIntSize(size));
}
//!move from memory to reg

//!Operands are already allocated
LowOpRegMem* move_mem_to_reg_noalloc(OpndSize size,
                  int disp, int base_reg, bool isBasePhysical,
                  MemoryAccessType mType, int mIndex,
                  int reg, bool isPhysical) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    return dump_mem_reg_noalloc(m, size, disp, base_reg, isBasePhysical, mType, mIndex, reg, isPhysical, getTypeFromIntSize(size));
}
//!movss from memory to reg

//!Operands are already allocated
LowOpRegMem* move_ss_mem_to_reg_noalloc(int disp, int base_reg, bool isBasePhysical,
                 MemoryAccessType mType, int mIndex,
                 int reg, bool isPhysical) {
    return dump_mem_reg_noalloc(Mnemonic_MOVSS, OpndSize_32, disp, base_reg, isBasePhysical, mType, mIndex, reg, isPhysical, LowOpndRegType_xmm);
}
//!movss from reg to memory

//!Operands are already allocated
LowOpMemReg* move_ss_reg_to_mem_noalloc(int reg, bool isPhysical,
                 int disp, int base_reg, bool isBasePhysical,
                 MemoryAccessType mType, int mIndex) {
    return dump_reg_mem_noalloc(Mnemonic_MOVSS, OpndSize_32, reg, isPhysical, disp, base_reg, isBasePhysical, mType, mIndex, LowOpndRegType_xmm);
}
//!movzx from memory to reg

//!
void movez_mem_to_reg(OpndSize size,
               int disp, int base_reg, bool isBasePhysical,
               int reg, bool isPhysical) {
    Mnemonic m = Mnemonic_MOVZX;
    dump_movez_mem_reg(m, size, disp, base_reg, isBasePhysical, reg, isPhysical);
}

//!movzx from one reg to another reg

//!
void movez_reg_to_reg(OpndSize size,
                      int reg, bool isPhysical,
                      int reg2, bool isPhysical2) {
    Mnemonic m = Mnemonic_MOVZX;
    dump_movez_reg_reg(m, size, reg, isPhysical, reg2, isPhysical2);
}

void movez_mem_disp_scale_to_reg(OpndSize size,
                 int base_reg, bool isBasePhysical,
                 int disp, int index_reg, bool isIndexPhysical, int scale,
                 int reg, bool isPhysical) {
    dump_mem_scale_reg(Mnemonic_MOVZX, size, base_reg, isBasePhysical,
                 disp, index_reg, isIndexPhysical, scale,
                 reg, isPhysical, LowOpndRegType_gp);
}
void moves_mem_disp_scale_to_reg(OpndSize size,
                  int base_reg, bool isBasePhysical,
                  int disp, int index_reg, bool isIndexPhysical, int scale,
                  int reg, bool isPhysical) {
    dump_mem_scale_reg(Mnemonic_MOVSX, size, base_reg, isBasePhysical,
                  disp, index_reg, isIndexPhysical, scale,
                  reg, isPhysical, LowOpndRegType_gp);
}

//!movsx from memory to reg

//!
void moves_mem_to_reg(LowOp* op, OpndSize size,
               int disp, int base_reg, bool isBasePhysical,
               int reg, bool isPhysical) {
    Mnemonic m = Mnemonic_MOVSX;
    dump_moves_mem_reg(m, size, disp, base_reg, isBasePhysical, reg, isPhysical);
}
//!mov from one reg to another reg

//!
void move_reg_to_reg(OpndSize size,
                      int reg, bool isPhysical,
                      int reg2, bool isPhysical2) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    dump_reg_reg(m, ATOM_NORMAL, size, reg, isPhysical, reg2, isPhysical2, getTypeFromIntSize(size));
}
//!mov from one reg to another reg

//!Operands are already allocated
void move_reg_to_reg_noalloc(OpndSize size,
                  int reg, bool isPhysical,
                  int reg2, bool isPhysical2) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    dump_reg_reg_noalloc(m, size, reg, isPhysical, reg2, isPhysical2, getTypeFromIntSize(size));
}
//!move from memory to reg

//!
void move_mem_scale_to_reg(OpndSize size,
                int base_reg, bool isBasePhysical, int index_reg, bool isIndexPhysical, int scale,
                int reg, bool isPhysical) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    dump_mem_scale_reg(m, size, base_reg, isBasePhysical, 0/*disp*/, index_reg, isIndexPhysical, scale,
                              reg, isPhysical, getTypeFromIntSize(size));
}
void move_mem_disp_scale_to_reg(OpndSize size,
                int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale,
                int reg, bool isPhysical) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    dump_mem_scale_reg(m, size, base_reg, isBasePhysical, disp, index_reg, isIndexPhysical, scale,
                              reg, isPhysical, getTypeFromIntSize(size));
}
//!move from reg to memory

//!
void move_reg_to_mem_scale(OpndSize size,
                int reg, bool isPhysical,
                int base_reg, bool isBasePhysical, int index_reg, bool isIndexPhysical, int scale) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    dump_reg_mem_scale(m, size, reg, isPhysical,
                              base_reg, isBasePhysical, 0/*disp*/, index_reg, isIndexPhysical, scale,
                              getTypeFromIntSize(size));
}
void move_reg_to_mem_disp_scale(OpndSize size,
                int reg, bool isPhysical,
                int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    dump_reg_mem_scale(m, size, reg, isPhysical,
                              base_reg, isBasePhysical, disp, index_reg, isIndexPhysical, scale,
                              getTypeFromIntSize(size));
}

void move_chain_to_mem(OpndSize size, int imm,
                        int disp, int base_reg, bool isBasePhysical) {
    dump_imm_mem(Mnemonic_MOV, ATOM_NORMAL, size, imm, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, true);
}

//!move an immediate to memory

//!
void move_imm_to_mem(OpndSize size, int imm,
                      int disp, int base_reg, bool isBasePhysical) {
    assert(size != OpndSize_64);
    if(size == OpndSize_64) ALOGE("move_imm_to_mem with 64 bits");
    dump_imm_mem(Mnemonic_MOV, ATOM_NORMAL, size, imm, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, false);
}
//! set a VR to an immediate

//!
void set_VR_to_imm(u2 vA, OpndSize size, int imm) {
    assert(size != OpndSize_64);
    if(size == OpndSize_64) ALOGE("move_imm_to_mem with 64 bits");
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        int regAll = checkVirtualReg(vA, getTypeFromIntSize(size), 0);
        if(regAll != PhysicalReg_Null) {
            dump_imm_reg_noalloc(m, size, imm, regAll, true, LowOpndRegType_gp);
            updateRefCount(vA, getTypeFromIntSize(size));
            updateVirtualReg(vA, getTypeFromIntSize(size));
            return;
        }
        //will call freeReg
        freeReg(true);
        regAll = registerAlloc(LowOpndRegType_virtual | getTypeFromIntSize(size), vA, false/*dummy*/, true);
        if(regAll == PhysicalReg_Null) {
            dump_imm_mem_noalloc(m, size, imm, 4*vA, PhysicalReg_FP, true, MemoryAccess_VR, vA);
            return;
        }
        dump_imm_reg_noalloc(m, size, imm, regAll, true, LowOpndRegType_gp);
        updateVirtualReg(vA, getTypeFromIntSize(size));
    }
    else {
        dump_imm_mem(m, ATOM_NORMAL, size, imm, 4*vA, PhysicalReg_FP, true, MemoryAccess_VR, vA, false);
    }
}
void set_VR_to_imm_noupdateref(LowOp* op, u2 vA, OpndSize size, int imm) {
    return;
}
//! set a VR to an immediate

//! Do not allocate a physical register for the VR
void set_VR_to_imm_noalloc(u2 vA, OpndSize size, int imm) {
    assert(size != OpndSize_64);
    if(size == OpndSize_64) ALOGE("move_imm_to_mem with 64 bits");
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    dump_imm_mem_noalloc(m, size, imm, 4*vA, PhysicalReg_FP, true, MemoryAccess_VR, vA);
}

void move_chain_to_reg(OpndSize size, int imm, int reg, bool isPhysical) {
    dump_imm_reg(Mnemonic_MOV, ATOM_NORMAL, size, imm, reg, isPhysical, LowOpndRegType_gp, true);
}

//! move an immediate to reg

//!
void move_imm_to_reg(OpndSize size, int imm, int reg, bool isPhysical) {
    assert(size != OpndSize_64);
    if(size == OpndSize_64) ALOGE("move_imm_to_reg with 64 bits");
    Mnemonic m = Mnemonic_MOV;
    dump_imm_reg(m, ATOM_NORMAL, size, imm, reg, isPhysical, LowOpndRegType_gp, false);
}
//! move an immediate to reg

//! The operand is already allocated
void move_imm_to_reg_noalloc(OpndSize size, int imm, int reg, bool isPhysical) {
    assert(size != OpndSize_64);
    if(size == OpndSize_64) ALOGE("move_imm_to_reg with 64 bits");
    Mnemonic m = Mnemonic_MOV;
    dump_imm_reg_noalloc(m, size, imm, reg, isPhysical, LowOpndRegType_gp);
}
//!cmov from reg to reg

//!
void conditional_move_reg_to_reg(OpndSize size, ConditionCode cc, int reg1, bool isPhysical1, int reg, bool isPhysical) {
    Mnemonic m = (Mnemonic)(Mnemonic_CMOVcc+cc);
    dump_reg_reg(m, ATOM_NORMAL, size, reg1, isPhysical1, reg, isPhysical, LowOpndRegType_gp);
}
//!movss from memory to reg

//!
void move_ss_mem_to_reg(LowOp* op, int disp, int base_reg, bool isBasePhysical,
                         int reg, bool isPhysical) {
    dump_mem_reg(Mnemonic_MOVSS, ATOM_NORMAL, OpndSize_32, disp, base_reg, isBasePhysical,
        MemoryAccess_Unknown, -1, reg, isPhysical, LowOpndRegType_xmm);
}
//!movss from reg to memory

//!
void move_ss_reg_to_mem(LowOp* op, int reg, bool isPhysical,
                         int disp, int base_reg, bool isBasePhysical) {
    dump_reg_mem(Mnemonic_MOVSS, ATOM_NORMAL, OpndSize_32, reg, isPhysical, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, LowOpndRegType_xmm);
}
//!movsd from memory to reg

//!
void move_sd_mem_to_reg(int disp, int base_reg, bool isBasePhysical,
                         int reg, bool isPhysical) {
    dump_mem_reg(Mnemonic_MOVSD, ATOM_NORMAL, OpndSize_64, disp, base_reg, isBasePhysical, MemoryAccess_Unknown, -1, reg, isPhysical, LowOpndRegType_xmm);
}
//!movsd from reg to memory

//!
void move_sd_reg_to_mem(LowOp* op, int reg, bool isPhysical,
                         int disp, int base_reg, bool isBasePhysical) {
    dump_reg_mem(Mnemonic_MOVSD, ATOM_NORMAL, OpndSize_64, reg, isPhysical,
                        disp, base_reg, isBasePhysical,
                        MemoryAccess_Unknown, -1, LowOpndRegType_xmm);
}
//!load from VR to a temporary

//!
void get_virtual_reg_all(u2 vB, OpndSize size, int reg, bool isPhysical, Mnemonic m) {
    LowOpndRegType type = getTypeFromIntSize(size);
    LowOpndRegType pType = type;//gp or xmm
    OpndSize size2 = size;
    Mnemonic m2 = m;
    if(m == Mnemonic_MOVSS) {
        size = OpndSize_32;
        size2 = OpndSize_64;
        type = LowOpndRegType_ss;
        pType = LowOpndRegType_xmm;
        m2 = Mnemonic_MOVQ; //to move from one xmm register to another
    }
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        int tmpValue[2];
        int isConst;
        isConst = isVirtualRegConstant(vB, type, tmpValue, true/*updateRefCount*/);
        if(isConst == 3) {
            if(m == Mnemonic_MOVSS) { //load 32 bits from VR
                //VR is not mapped to a register but in memory
                dumpImmToMem(vB, OpndSize_32, tmpValue[0]);
                //temporary reg has "pType" (which is xmm)
                dump_mem_reg(m, ATOM_NORMAL, size, 4*vB, PhysicalReg_FP, true,
                    MemoryAccess_VR, vB, reg, isPhysical, pType);
                return;
            }
            else if(m == Mnemonic_MOVSD || size == OpndSize_64) {
                //VR is not mapped to a register but in memory
                dumpImmToMem(vB, OpndSize_32, tmpValue[0]);
                dumpImmToMem(vB+1, OpndSize_32, tmpValue[1]);
                dump_mem_reg(m, ATOM_NORMAL, size, 4*vB, PhysicalReg_FP, true,
                    MemoryAccess_VR, vB, reg, isPhysical, pType);
                return;
            }
            else if(size != OpndSize_64) {
                //VR is not mapped to a register
                dump_imm_reg(m, ATOM_NORMAL, size, tmpValue[0], reg, isPhysical, pType, false);
                return;
            }
        }
        if(isConst == 1) dumpImmToMem(vB, OpndSize_32, tmpValue[0]);
        if(isConst == 2) dumpImmToMem(vB+1, OpndSize_32, tmpValue[1]);
        freeReg(true);
        int regAll = checkVirtualReg(vB, type, 0);
        if(regAll != PhysicalReg_Null) {
            startNativeCode(vB, type);
            donotSpillReg(regAll);
            //check XFER_MEM_TO_XMM
            updateVRAtUse(vB, type, regAll);
            //temporary reg has "pType"
            dump_reg_reg_noalloc_src(m2, ATOM_NORMAL, size2, regAll, true, reg, isPhysical, pType); //register allocator handles assembly move
            endNativeCode();
            updateRefCount(vB, type);
            return;
        }
        //not allocated to a register yet, no need to check XFER_MEM_TO_XMM
        regAll = registerAlloc(LowOpndRegType_virtual | type, vB, false/*dummy*/, false);
        if(regAll == PhysicalReg_Null) {
            dump_mem_reg_noalloc(m, size, 4*vB, PhysicalReg_FP, true,
                MemoryAccess_VR, vB, reg, isPhysical, pType);
            return;
        }

        //temporary reg has pType
        if(checkTempReg2(reg, pType, isPhysical, regAll)) {
            registerAllocMove(reg, pType, isPhysical, regAll);
            dump_mem_reg_noalloc(m, size, 4*vB, PhysicalReg_FP, true,
                MemoryAccess_VR, vB, regAll, true, pType);
            updateRefCount(vB, type);
            return;
        }
        else {
            dump_mem_reg_noalloc(m, size, 4*vB, PhysicalReg_FP, true,
                MemoryAccess_VR, vB, regAll, true, pType);
            //xmm with 32 bits
            startNativeCode(vB, type);
            donotSpillReg(regAll);
            dump_reg_reg_noalloc_src(m2, ATOM_NORMAL, size2, regAll, true, reg, isPhysical, pType);
            endNativeCode();
            updateRefCount(vB, type);
            return;
        }
    }
    else {
        dump_mem_reg(m, ATOM_NORMAL, size, 4*vB, PhysicalReg_FP, true,
            MemoryAccess_VR, vB, reg, isPhysical, pType);
    }
}
void get_virtual_reg(u2 vB, OpndSize size, int reg, bool isPhysical) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    return get_virtual_reg_all(vB, size, reg, isPhysical, m);
}
void get_virtual_reg_noalloc(u2 vB, OpndSize size, int reg, bool isPhysical) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    dump_mem_reg_noalloc(m, size, 4*vB, PhysicalReg_FP, true,
        MemoryAccess_VR, vB, reg, isPhysical, getTypeFromIntSize(size));
}
//3 cases: gp, xmm, ss
//ss: the temporary register is xmm
//!load from a temporary to a VR

//!
void set_virtual_reg_all(u2 vA, OpndSize size, int reg, bool isPhysical, Mnemonic m) {
    LowOpndRegType type = getTypeFromIntSize(size);
    LowOpndRegType pType = type;//gp or xmm
    OpndSize size2 = size;
    Mnemonic m2 = m;
    if(m == Mnemonic_MOVSS) {
        size = OpndSize_32;
        size2 = OpndSize_64;
        type = LowOpndRegType_ss;
        pType = LowOpndRegType_xmm;
        m2 = Mnemonic_MOVQ;
    }
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        //3 cases
        //1: virtual register is already allocated to a physical register
        //   call dump_reg_reg_noalloc_dst
        //2: src reg is already allocated, VR is not yet allocated
        //   allocate VR to the same physical register used by src reg
        //   [call registerAllocMove]
        //3: both not yet allocated
        //   allocate a physical register for the VR
        //   then call dump_reg_reg_noalloc_dst
        //may need to convert from gp to xmm or the other way
        freeReg(true);
        int regAll = checkVirtualReg(vA, type, 0);
        if(regAll != PhysicalReg_Null)  { //case 1
            startNativeCode(-1, -1);
            donotSpillReg(regAll);
            dump_reg_reg_noalloc_dst(m2, size2, reg, isPhysical, regAll, true, pType); //temporary reg is "pType"
            endNativeCode();
            updateRefCount(vA, type);
            updateVirtualReg(vA, type); //will dump VR to memory, should happen afterwards
            return;
        }
        regAll = checkTempReg(reg, pType, isPhysical, vA); //vA is not used inside
        if(regAll != PhysicalReg_Null) { //case 2
            registerAllocMove(vA, LowOpndRegType_virtual | type, false, regAll);
            updateVirtualReg(vA, type); //will dump VR to memory, should happen afterwards
            return; //next native instruction starts at op
        }
        //case 3
        regAll = registerAlloc(LowOpndRegType_virtual | type, vA, false/*dummy*/, false);
        if(regAll == PhysicalReg_Null) {
            dump_reg_mem_noalloc(m, size, reg, isPhysical, 4*vA, PhysicalReg_FP, true,
                MemoryAccess_VR, vA, pType);
            return;
        }
        startNativeCode(-1, -1);
        donotSpillReg(regAll);
        dump_reg_reg_noalloc_dst(m2, size2, reg, isPhysical, regAll, true, pType);
        endNativeCode();
        updateRefCount(vA, type);
        updateVirtualReg(vA, type);
    }
    else {
        dump_reg_mem(m, ATOM_NORMAL, size, reg, isPhysical, 4*vA, PhysicalReg_FP, true,
            MemoryAccess_VR, vA, pType);
    }
}
void set_virtual_reg(u2 vA, OpndSize size, int reg, bool isPhysical) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    return set_virtual_reg_all(vA, size, reg, isPhysical, m);
}
void set_virtual_reg_noalloc(u2 vA, OpndSize size, int reg, bool isPhysical) {
    Mnemonic m = (size == OpndSize_64) ? Mnemonic_MOVQ : Mnemonic_MOV;
    dump_reg_mem_noalloc(m, size, reg, isPhysical, 4*vA, PhysicalReg_FP, true,
        MemoryAccess_VR, vA, getTypeFromIntSize(size));
}
void get_VR_ss(int vB, int reg, bool isPhysical) {
    return get_virtual_reg_all(vB, OpndSize_64, reg, isPhysical, Mnemonic_MOVSS);
}
void set_VR_ss(int vA, int reg, bool isPhysical) {
    return set_virtual_reg_all(vA, OpndSize_64, reg, isPhysical, Mnemonic_MOVSS);
}
void get_VR_sd(int vB, int reg, bool isPhysical) {
    return get_virtual_reg_all(vB, OpndSize_64, reg, isPhysical, Mnemonic_MOVSD);
}
void set_VR_sd(int vA, int reg, bool isPhysical) {
    return set_virtual_reg_all(vA, OpndSize_64, reg, isPhysical, Mnemonic_MOVSD);
}
////////////////////////////////// END: IA32 native instructions //////////////
//! generate native instructions to get current PC in the stack frame

//!
int get_currentpc(int reg, bool isPhysical) {
    move_mem_to_reg(OpndSize_32, -sizeofStackSaveArea+offStackSaveArea_localRefTop, PhysicalReg_FP, true, reg, isPhysical);
    return 1;
}
//!generate native code to perform null check

//!This function does not export PC
int simpleNullCheck(int reg, bool isPhysical, int vr) {
    if(isVRNullCheck(vr, OpndSize_32)) {
        updateRefCount2(reg, LowOpndRegType_gp, isPhysical);
        num_removed_nullCheck++;
        return 0;
    }
    compare_imm_reg(OpndSize_32, 0, reg, isPhysical);
    conditional_jump_global_API(Condition_E, "common_errNullObject", false);
    setVRNullCheck(vr, OpndSize_32);
    return 0;
}

/* only for O1 code generator */
int boundCheck(int vr_array, int reg_array, bool isPhysical_array,
               int vr_index, int reg_index, bool isPhysical_index,
               int exceptionNum) {
#ifdef BOUNDCHECK_OPT
    if(isVRBoundCheck(vr_array, vr_index)) {
        updateRefCount2(reg_array, LowOpndRegType_gp, isPhysical_array);
        updateRefCount2(reg_index, LowOpndRegType_gp, isPhysical_index);
        return 0;
    }
#endif
    compare_mem_reg(OpndSize_32, offArrayObject_length,
                    reg_array, isPhysical_array,
                    reg_index, isPhysical_index);

    char errName[256];
    sprintf(errName, "common_errArrayIndex");
    handlePotentialException(
                                       Condition_NC, Condition_C,
                                       exceptionNum, errName);
#ifdef BOUNDCHECK_OPT
    setVRBoundCheck(vr_array, vr_index);
#endif
    return 0;
}

//!generate native code to perform null check

//!
int nullCheck(int reg, bool isPhysical, int exceptionNum, int vr) {
    char label[LABEL_SIZE];

    if(gDvm.executionMode == kExecutionModeNcgO1) {
        //nullCheck optimization is available in O1 mode only
        if(isVRNullCheck(vr, OpndSize_32)) {
            updateRefCount2(reg, LowOpndRegType_gp, isPhysical);
            if(exceptionNum <= 1) {
                updateRefCount2(PhysicalReg_EDX, LowOpndRegType_gp, true);
                updateRefCount2(PhysicalReg_EDX, LowOpndRegType_gp, true);
            }
            num_removed_nullCheck++;
            return 0;
        }
        compare_imm_reg(OpndSize_32, 0, reg, isPhysical);
        rememberState(exceptionNum);
        snprintf(label, LABEL_SIZE, "after_exception_%d", exceptionNum);
        conditional_jump(Condition_NE, label, true);
        if(exceptionNum > 1)
            nextVersionOfHardReg(PhysicalReg_EDX, 2); //next version has 2 ref count
        export_pc(); //use %edx
        constVREndOfBB();
        beforeCall("exception"); //dump GG, GL VRs
        unconditional_jump_global_API("common_errNullObject", false);
        insertLabel(label, true);
        goToState(exceptionNum);
        setVRNullCheck(vr, OpndSize_32);
    } else {
        compare_imm_reg(OpndSize_32, 0, reg, isPhysical);
        snprintf(label, LABEL_SIZE, "after_exception_%d", exceptionNum);
        conditional_jump(Condition_NE, label, true);
        export_pc(); //use %edx
        unconditional_jump_global_API("common_errNullObject", false);
        insertLabel(label, true);
    }
    return 0;
}
//!generate native code to handle potential exception

//!
int handlePotentialException(
                             ConditionCode code_excep, ConditionCode code_okay,
                             int exceptionNum, const char* errName) {
    char label[LABEL_SIZE];

    if(gDvm.executionMode == kExecutionModeNcgO1) {
        rememberState(exceptionNum);
        snprintf(label, LABEL_SIZE, "after_exception_%d", exceptionNum);
        conditional_jump(code_okay, label, true);
        if(exceptionNum > 1)
            nextVersionOfHardReg(PhysicalReg_EDX, 2); //next version has 2 ref count
        export_pc(); //use %edx
        constVREndOfBB();
        beforeCall("exception"); //dump GG, GL VRs
        if(!strcmp(errName, "common_throw_message")) {
            move_imm_to_reg(OpndSize_32, LstrInstantiationErrorPtr, PhysicalReg_ECX, true);
        }
        unconditional_jump_global_API(errName, false);
        insertLabel(label, true);
        goToState(exceptionNum);
    } else {
        snprintf(label, LABEL_SIZE, "after_exception_%d", exceptionNum);
        conditional_jump(code_okay, label, true);
        export_pc(); //use %edx
        if(!strcmp(errName, "common_throw_message")) {
            move_imm_to_reg(OpndSize_32, LstrInstantiationErrorPtr, PhysicalReg_ECX, true);
        }
        unconditional_jump_global_API(errName, false);
        insertLabel(label, true);
    }
    return 0;
}
//!generate native code to get the self pointer from glue

//!It uses one scratch register
int get_self_pointer(int reg, bool isPhysical) {
    move_mem_to_reg(OpndSize_32, offEBP_self, PhysicalReg_EBP, true, reg, isPhysical);
    return 0;
}
//!generate native code to get ResStrings from glue

//!It uses two scratch registers
int get_res_strings(int reg, bool isPhysical) {
    //if spill_loc_index > 0 || reg != NULL, use registerAlloc
    if(isGlueHandled(PhysicalReg_GLUE_DVMDEX)) {
        //if spill_loc_index > 0
        //  load from spilled location, update spill_loc_index & physicalReg
#if 0
        updateRefCount2(C_SCRATCH_1, LowOpndRegType_gp, isScratchPhysical);
        updateRefCount2(C_SCRATCH_1, LowOpndRegType_gp, isScratchPhysical);
        updateRefCount2(C_SCRATCH_2, LowOpndRegType_gp, isScratchPhysical);
        updateRefCount2(C_SCRATCH_2, LowOpndRegType_gp, isScratchPhysical);
#endif
        startNativeCode(-1, -1);
        freeReg(true);
        int regAll = registerAlloc(LowOpndRegType_gp, PhysicalReg_GLUE_DVMDEX, false, false/*updateRefCount*/);
        donotSpillReg(regAll);
        dump_mem_reg_noalloc_mem(Mnemonic_MOV, ATOM_NORMAL, OpndSize_32, offDvmDex_pResStrings, regAll, true, MemoryAccess_Unknown, -1, reg, isPhysical, LowOpndRegType_gp);
        endNativeCode();
    }
    else
        {
            get_self_pointer(C_SCRATCH_1, isScratchPhysical);
            move_mem_to_reg(OpndSize_32, offsetof(Thread, interpSave.methodClassDex), C_SCRATCH_1, isScratchPhysical, C_SCRATCH_2, isScratchPhysical);
            //glue is not in a physical reg nor in a spilled location
            updateGlue(C_SCRATCH_2, isScratchPhysical, PhysicalReg_GLUE_DVMDEX); //spill_loc_index is -1, set physicalReg
            move_mem_to_reg(OpndSize_32, offDvmDex_pResStrings, C_SCRATCH_2, isScratchPhysical, reg, isPhysical);
        }
    return 0;
}
int get_res_classes(int reg, bool isPhysical) {
    //if spill_loc_index > 0 || reg != NULL, use registerAlloc
    if(isGlueHandled(PhysicalReg_GLUE_DVMDEX)) {
        //if spill_loc_index > 0
        //  load from spilled location, updte spill_loc_index & physicalReg
        startNativeCode(-1, -1);
        freeReg(true);
        int regAll = registerAlloc(LowOpndRegType_gp, PhysicalReg_GLUE_DVMDEX, false, false/*updateRefCount*/);
        donotSpillReg(regAll);
        dump_mem_reg_noalloc_mem(Mnemonic_MOV, ATOM_NORMAL, OpndSize_32, offDvmDex_pResClasses, regAll, true, MemoryAccess_Unknown, -1, reg, isPhysical, LowOpndRegType_gp);
        endNativeCode();
    }
    else
        {
            get_self_pointer(C_SCRATCH_1, isScratchPhysical);
            move_mem_to_reg(OpndSize_32, offsetof(Thread, interpSave.methodClassDex), C_SCRATCH_1, isScratchPhysical, C_SCRATCH_2, isScratchPhysical);
            //glue is not in a physical reg nor in a spilled location
            updateGlue(C_SCRATCH_2, isScratchPhysical, PhysicalReg_GLUE_DVMDEX); //spill_loc_index is -1, set physicalReg
            move_mem_to_reg(OpndSize_32, offDvmDex_pResClasses, C_SCRATCH_2, isScratchPhysical, reg, isPhysical);
        }
    return 0;
}
//!generate native code to get ResFields from glue

//!It uses two scratch registers
int get_res_fields(int reg, bool isPhysical) {
    //if spill_loc_index > 0 || reg != NULL, use registerAlloc
    if(isGlueHandled(PhysicalReg_GLUE_DVMDEX)) {
        //if spill_loc_index > 0
        //  load from spilled location, updte spill_loc_index & physicalReg
        startNativeCode(-1, -1);
        freeReg(true);
        int regAll = registerAlloc(LowOpndRegType_gp, PhysicalReg_GLUE_DVMDEX, false, false/*updateRefCount*/);
        donotSpillReg(regAll);
        dump_mem_reg_noalloc_mem(Mnemonic_MOV, ATOM_NORMAL, OpndSize_32, offDvmDex_pResFields, regAll, true, MemoryAccess_Unknown, -1, reg, isPhysical, LowOpndRegType_gp);
        endNativeCode();
    }
    else
        {
            get_self_pointer(C_SCRATCH_1, isScratchPhysical);
            move_mem_to_reg(OpndSize_32, offsetof(Thread, interpSave.methodClassDex), C_SCRATCH_1, isScratchPhysical, C_SCRATCH_2, isScratchPhysical);
            //glue is not in a physical reg nor in a spilled location
            updateGlue(C_SCRATCH_2, isScratchPhysical, PhysicalReg_GLUE_DVMDEX); //spill_loc_index is -1, set physicalReg
            move_mem_to_reg(OpndSize_32, offDvmDex_pResFields, C_SCRATCH_2, isScratchPhysical, reg, isPhysical);
        }
    return 0;
}
//!generate native code to get ResMethods from glue

//!It uses two scratch registers
int get_res_methods(int reg, bool isPhysical) {
    //if spill_loc_index > 0 || reg != NULL, use registerAlloc
    if(isGlueHandled(PhysicalReg_GLUE_DVMDEX)) {
        //if spill_loc_index > 0
        //  load from spilled location, updte spill_loc_index & physicalReg
        startNativeCode(-1, -1);
        freeReg(true);
        int regAll = registerAlloc(LowOpndRegType_gp, PhysicalReg_GLUE_DVMDEX, false, false/*updateRefCount*/);
        donotSpillReg(regAll);
        dump_mem_reg_noalloc_mem(Mnemonic_MOV, ATOM_NORMAL, OpndSize_32, offDvmDex_pResMethods, regAll, true, MemoryAccess_Unknown, -1, reg, isPhysical, LowOpndRegType_gp);
        endNativeCode();
    }
    else
        {
            get_self_pointer(C_SCRATCH_1, isScratchPhysical);
            move_mem_to_reg(OpndSize_32, offsetof(Thread, interpSave.methodClassDex), C_SCRATCH_1, isScratchPhysical, C_SCRATCH_2, isScratchPhysical);
            //glue is not in a physical reg nor in a spilled location
            updateGlue(C_SCRATCH_2, isScratchPhysical, PhysicalReg_GLUE_DVMDEX); //spill_loc_index is -1, set physicalReg
            move_mem_to_reg(OpndSize_32, offDvmDex_pResMethods, C_SCRATCH_2, isScratchPhysical, reg, isPhysical);
        }
    return 0;
}
//!generate native code to get the current class object from glue

//!It uses two scratch registers
int get_glue_method_class(int reg, bool isPhysical) {
    get_self_pointer(C_SCRATCH_1, isScratchPhysical);
    move_mem_to_reg(OpndSize_32, offsetof(Thread, interpSave.method), C_SCRATCH_1, isScratchPhysical, C_SCRATCH_2, isScratchPhysical);
    move_mem_to_reg(OpndSize_32, offMethod_clazz, C_SCRATCH_2, isScratchPhysical, reg, isPhysical);
    return 0;
}
//!generate native code to get the current method from glue

//!It uses one scratch register
int get_glue_method(int reg, bool isPhysical) {
    get_self_pointer(C_SCRATCH_1, isScratchPhysical);
    move_mem_to_reg(OpndSize_32, offsetof(Thread, interpSave.method), C_SCRATCH_1, isScratchPhysical, reg, isPhysical);
    return 0;
}
//!generate native code to set the current method in glue

//!It uses one scratch register
int set_glue_method(int reg, bool isPhysical) {
    get_self_pointer(C_SCRATCH_1, isScratchPhysical);
    move_reg_to_mem(OpndSize_32, reg, isPhysical, offsetof(Thread, interpSave.method), C_SCRATCH_1, isScratchPhysical);
    return 0;
}

//!generate native code to get DvmDex from glue

//!It uses one scratch register
int get_glue_dvmdex(int reg, bool isPhysical) {
    //if spill_loc_index > 0 || reg != NULL, use registerAlloc
    if(isGlueHandled(PhysicalReg_GLUE_DVMDEX)) {
        //if spill_loc_index > 0
        //  load from spilled location, updte spill_loc_index & physicalReg
        startNativeCode(-1, -1);
        freeReg(true);
        int regAll = registerAlloc(LowOpndRegType_gp, PhysicalReg_GLUE_DVMDEX, false, false/*updateRefCount*/);
        donotSpillReg(regAll);
        dump_reg_reg_noalloc_src(Mnemonic_MOV, ATOM_NORMAL, OpndSize_32, regAll, true,
                                          reg, isPhysical, LowOpndRegType_gp);
        endNativeCode();
    }
    else
        {
            get_self_pointer(C_SCRATCH_1, isScratchPhysical);
            move_mem_to_reg(OpndSize_32, offsetof(Thread, interpSave.methodClassDex), C_SCRATCH_1, isScratchPhysical, reg, isPhysical);
            //glue is not in a physical reg nor in a spilled location
            updateGlue(reg, isPhysical, PhysicalReg_GLUE_DVMDEX); //spill_loc_index is -1, set physicalReg
        }
    return 0;
}
//!generate native code to set DvmDex in glue

//!It uses one scratch register
int set_glue_dvmdex(int reg, bool isPhysical) {
    get_self_pointer(C_SCRATCH_1, isScratchPhysical);
    move_reg_to_mem(OpndSize_32, reg, isPhysical, offsetof(Thread, interpSave.methodClassDex), C_SCRATCH_1, isScratchPhysical);
    return 0;
}
//!generate native code to get SuspendCount from glue

//!It uses one scratch register
int get_suspendCount(int reg, bool isPhysical) {
    get_self_pointer(C_SCRATCH_1, isScratchPhysical);
    move_mem_to_reg(OpndSize_32, offsetof(Thread, suspendCount), C_SCRATCH_1, isScratchPhysical, reg, isPhysical);
    return 0;
}

//!generate native code to get retval from glue

//!It uses one scratch register
int get_return_value(OpndSize size, int reg, bool isPhysical) {
    get_self_pointer(C_SCRATCH_1, isScratchPhysical);
    move_mem_to_reg(size, offsetof(Thread, interpSave.retval), C_SCRATCH_1, isScratchPhysical, reg, isPhysical);
    return 0;
}
//!generate native code to set retval in glue

//!It uses one scratch register
int set_return_value(OpndSize size, int reg, bool isPhysical) {
    get_self_pointer(C_SCRATCH_1, isScratchPhysical);
    move_reg_to_mem(size, reg, isPhysical, offsetof(Thread, interpSave.retval), C_SCRATCH_1, isScratchPhysical);
    return 0;
}
//!generate native code to clear exception object in glue

//!It uses two scratch registers
int clear_exception() {
    get_self_pointer(C_SCRATCH_2, isScratchPhysical);
    move_imm_to_mem(OpndSize_32, 0, offsetof(Thread, exception), C_SCRATCH_2, isScratchPhysical);
    return 0;
}
//!generate native code to get exception object in glue

//!It uses two scratch registers
int get_exception(int reg, bool isPhysical) {
    get_self_pointer(C_SCRATCH_2, isScratchPhysical);
    move_mem_to_reg(OpndSize_32, offsetof(Thread, exception), C_SCRATCH_2, isScratchPhysical, reg, isPhysical);
    return 0;
}
//!generate native code to set exception object in glue

//!It uses two scratch registers
int set_exception(int reg, bool isPhysical) {
    get_self_pointer(C_SCRATCH_2, isScratchPhysical);
    move_reg_to_mem(OpndSize_32, reg, isPhysical, offsetof(Thread, exception), C_SCRATCH_2, isScratchPhysical);
    return 0;
}
//!generate native code to save frame pointer and current PC in stack frame to glue

//!It uses two scratch registers
int save_pc_fp_to_glue() {
    get_self_pointer(C_SCRATCH_1, isScratchPhysical);
    move_reg_to_mem(OpndSize_32, PhysicalReg_FP, true, offsetof(Thread, interpSave.curFrame), C_SCRATCH_1, isScratchPhysical);

    //from stack-save currentPc
    move_mem_to_reg(OpndSize_32, -sizeofStackSaveArea+offStackSaveArea_localRefTop, PhysicalReg_FP, true, C_SCRATCH_2, isScratchPhysical);
    move_reg_to_mem(OpndSize_32, C_SCRATCH_2, isScratchPhysical, offsetof(Thread, interpSave.pc), C_SCRATCH_1, isScratchPhysical);
    return 0;
}
//! get SaveArea pointer

//!
int savearea_from_fp(int reg, bool isPhysical) {
    load_effective_addr(-sizeofStackSaveArea, PhysicalReg_FP, true, reg, isPhysical);
    return 0;
}

#ifdef DEBUG_CALL_STACK3
int call_debug_dumpSwitch() {
    typedef void (*vmHelper)(int);
    vmHelper funcPtr = debug_dumpSwitch;
    callFuncPtr((int)funcPtr, "debug_dumpSwitch");
    return 0;
}
#endif

int call_dvmQuasiAtomicSwap64() {
    typedef int64_t (*vmHelper)(int64_t, volatile int64_t*);
    vmHelper funcPtr = dvmQuasiAtomicSwap64;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmQuasiAtomicSwap64");
        callFuncPtr((int)funcPtr, "dvmQuasiAtomicSwap64");
        afterCall("dvmQuasiAtomicSwap64");
    } else {
        callFuncPtr((int)funcPtr, "dvmQuasiAtomicSwap64");
    }
    return 0;
}

int call_dvmQuasiAtomicRead64() {
    typedef int64_t (*vmHelper)(volatile const int64_t*);
    vmHelper funcPtr = dvmQuasiAtomicRead64;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmQuasiAtomiRead64");
        callFuncPtr((int)funcPtr, "dvmQuasiAtomicRead64");
        afterCall("dvmQuasiAtomicRead64");
        touchEax(); //for return value
        touchEdx();
    } else {
        callFuncPtr((int)funcPtr, "dvmQuasiAtomicRead64");
    }
    return 0;
}

int call_dvmJitToInterpPunt() {
    typedef void (*vmHelper)(int);
    vmHelper funcPtr = dvmJitToInterpPunt;
    callFuncPtr((int)funcPtr, "dvmJitToInterpPunt");
    return 0;
}

int call_dvmJitToInterpNormal() {
    typedef void (*vmHelper)(int);
    vmHelper funcPtr = dvmJitToInterpNormal;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmJitToInterpNormal");
        callFuncPtr((int)funcPtr, "dvmJitToInterpNormal");
        afterCall("dvmJitToInterpNormal");
        touchEbx();
    } else {
        callFuncPtr((int)funcPtr, "dvmJitToInterpNormal");
    }
    return 0;
}

int call_dvmJitToInterpTraceSelectNoChain() {
    typedef void (*vmHelper)(int);
    vmHelper funcPtr = dvmJitToInterpTraceSelectNoChain;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmJitToInterpTraceSelectNoChain");
        callFuncPtr((int)funcPtr, "dvmJitToInterpTraceSelectNoChain");
        afterCall("dvmJitToInterpTraceSelectNoChain");
        touchEbx();
    } else {
        callFuncPtr((int)funcPtr, "dvmJitToInterpTraceSelectNoChain");
    }
    return 0;
}

int call_dvmJitToInterpTraceSelect() {
    typedef void (*vmHelper)(int);
    vmHelper funcPtr = dvmJitToInterpTraceSelect;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmJitToInterpTraceSelect");
        callFuncPtr((int)funcPtr, "dvmJitToInterpTraceSelect");
        afterCall("dvmJitToInterpTraceSelect");
        touchEbx();
    } else {
        callFuncPtr((int)funcPtr, "dvmJitToInterpTraceSelect");
    }
    return 0;
}

int call_dvmJitToPatchPredictedChain() {
    typedef const Method * (*vmHelper)(const Method *method,
                                       Thread *self,
                                       PredictedChainingCell *cell,
                                       const ClassObject *clazz);
    vmHelper funcPtr = dvmJitToPatchPredictedChain;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmJitToPatchPredictedChain");
        callFuncPtr((int)funcPtr, "dvmJitToPatchPredictedChain");
        afterCall("dvmJitToPatchPredictedChain");
    } else {
        callFuncPtr((int)funcPtr, "dvmJitToPatchPredictedChain");
    }
    return 0;
}

//!generate native code to call __moddi3

//!
int call_moddi3() {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("moddi3");
        callFuncPtr((intptr_t)__moddi3, "__moddi3");
        afterCall("moddi3");
    } else {
        callFuncPtr((intptr_t)__moddi3, "__moddi3");
    }
    return 0;
}
//!generate native code to call __divdi3

//!
int call_divdi3() {
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("divdi3");
        callFuncPtr((intptr_t)__divdi3, "__divdi3");
        afterCall("divdi3");
    } else {
        callFuncPtr((intptr_t)__divdi3, "__divdi3");
    }
    return 0;
}

//!generate native code to call fmod

//!
int call_fmod() {
    typedef double (*libHelper)(double, double);
    libHelper funcPtr = fmod;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("fmod");
        callFuncPtr((int)funcPtr, "fmod");
        afterCall("fmod");
    } else {
        callFuncPtr((int)funcPtr, "fmod");
    }
    return 0;
}
//!generate native code to call fmodf

//!
int call_fmodf() {
    typedef float (*libHelper)(float, float);
    libHelper funcPtr = fmodf;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("fmodf");
        callFuncPtr((int)funcPtr, "fmodf");
        afterCall("fmodf");
    } else {
        callFuncPtr((int)funcPtr, "fmodf");
    }
    return 0;
}
//!generate native code to call dvmFindCatchBlock

//!
int call_dvmFindCatchBlock() {
    //int dvmFindCatchBlock(Thread* self, int relPc, Object* exception,
    //bool doUnroll, void** newFrame)
    typedef int (*vmHelper)(Thread*, int, Object*, bool, void**);
    vmHelper funcPtr = dvmFindCatchBlock;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmFindCatchBlock");
        callFuncPtr((int)funcPtr, "dvmFindCatchBlock");
        afterCall("dvmFindCatchBlock");
    } else {
        callFuncPtr((int)funcPtr, "dvmFindCatchBlock");
    }
    return 0;
}
//!generate native code to call dvmThrowVerificationError

//!
int call_dvmThrowVerificationError() {
    typedef void (*vmHelper)(const Method*, int, int);
    vmHelper funcPtr = dvmThrowVerificationError;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmThrowVerificationError");
        callFuncPtr((int)funcPtr, "dvmThrowVerificationError");
        afterCall("dvmThrowVerificationError");
    } else {
        callFuncPtr((int)funcPtr, "dvmThrowVerificationError");
    }
    return 0;
}

//!generate native code to call dvmResolveMethod

//!
int call_dvmResolveMethod() {
    //Method* dvmResolveMethod(const ClassObject* referrer, u4 methodIdx, MethodType methodType);
    typedef Method* (*vmHelper)(const ClassObject*, u4, MethodType);
    vmHelper funcPtr = dvmResolveMethod;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmResolveMethod");
        callFuncPtr((int)funcPtr, "dvmResolveMethod");
        afterCall("dvmResolveMethod");
    } else {
        callFuncPtr((int)funcPtr, "dvmResolveMethod");
    }
    return 0;
}
//!generate native code to call dvmResolveClass

//!
int call_dvmResolveClass() {
    //ClassObject* dvmResolveClass(const ClassObject* referrer, u4 classIdx, bool fromUnverifiedConstant)
    typedef ClassObject* (*vmHelper)(const ClassObject*, u4, bool);
    vmHelper funcPtr = dvmResolveClass;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmResolveClass");
        callFuncPtr((int)funcPtr, "dvmResolveClass");
        afterCall("dvmResolveClass");
    } else {
        callFuncPtr((int)funcPtr, "dvmResolveClass");
    }
    return 0;
}

//!generate native code to call dvmInstanceofNonTrivial

//!
int call_dvmInstanceofNonTrivial() {
    typedef int (*vmHelper)(const ClassObject*, const ClassObject*);
    vmHelper funcPtr = dvmInstanceofNonTrivial;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmInstanceofNonTrivial");
        callFuncPtr((int)funcPtr, "dvmInstanceofNonTrivial");
        afterCall("dvmInstanceofNonTrivial");
    } else {
        callFuncPtr((int)funcPtr, "dvmInstanceofNonTrivial");
    }
    return 0;
}
//!generate native code to call dvmThrowException

//!
int call_dvmThrow() {
    typedef void (*vmHelper)(ClassObject* exceptionClass, const char*);
    vmHelper funcPtr = dvmThrowException;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmThrowException");
        callFuncPtr((int)funcPtr, "dvmThrowException");
        afterCall("dvmThrowException");
    } else {
        callFuncPtr((int)funcPtr, "dvmThrowException");
    }
    return 0;
}
//!generate native code to call dvmThrowExceptionWithClassMessage

//!
int call_dvmThrowWithMessage() {
    typedef void (*vmHelper)(ClassObject* exceptionClass, const char*);
    vmHelper funcPtr = dvmThrowExceptionWithClassMessage;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmThrowExceptionWithClassMessage");
        callFuncPtr((int)funcPtr, "dvmThrowExceptionWithClassMessage");
        afterCall("dvmThrowExceptionWithClassMessage");
    } else {
        callFuncPtr((int)funcPtr, "dvmThrowExceptionWithClassMessage");
    }
    return 0;
}
//!generate native code to call dvmCheckSuspendPending

//!
int call_dvmCheckSuspendPending() {
    typedef bool (*vmHelper)(Thread*);
    vmHelper funcPtr = dvmCheckSuspendPending;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmCheckSuspendPending");
        callFuncPtr((int)funcPtr, "dvmCheckSuspendPending");
        afterCall("dvmCheckSuspendPending");
    } else {
        callFuncPtr((int)funcPtr, "dvmCheckSuspendPending");
    }
    return 0;
}
//!generate native code to call dvmLockObject

//!
int call_dvmLockObject() {
    typedef void (*vmHelper)(struct Thread*, struct Object*);
    vmHelper funcPtr = dvmLockObject;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmLockObject");
        callFuncPtr((int)funcPtr, "dvmLockObject");
        afterCall("dvmLockObject");
    } else {
        callFuncPtr((int)funcPtr, "dvmLockObject");
    }
    return 0;
}
//!generate native code to call dvmUnlockObject

//!
int call_dvmUnlockObject() {
    typedef bool (*vmHelper)(Thread*, Object*);
    vmHelper funcPtr = dvmUnlockObject;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmUnlockObject");
        callFuncPtr((int)funcPtr, "dvmUnlockObject");
        afterCall("dvmUnlockObject");
    } else {
        callFuncPtr((int)funcPtr, "dvmUnlockObject");
    }
    return 0;
}
//!generate native code to call dvmInitClass

//!
int call_dvmInitClass() {
    typedef bool (*vmHelper)(ClassObject*);
    vmHelper funcPtr = dvmInitClass;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmInitClass");
        callFuncPtr((int)funcPtr, "dvmInitClass");
        afterCall("dvmInitClass");
    } else {
        callFuncPtr((int)funcPtr, "dvmInitClass");
    }
    return 0;
}
//!generate native code to call dvmAllocObject

//!
int call_dvmAllocObject() {
    typedef Object* (*vmHelper)(ClassObject*, int);
    vmHelper funcPtr = dvmAllocObject;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmAllocObject");
        callFuncPtr((int)funcPtr, "dvmAllocObject");
        afterCall("dvmAllocObject");
    } else {
        callFuncPtr((int)funcPtr, "dvmAllocObject");
    }
    return 0;
}
//!generate native code to call dvmAllocArrayByClass

//!
int call_dvmAllocArrayByClass() {
    typedef ArrayObject* (*vmHelper)(ClassObject*, size_t, int);
    vmHelper funcPtr = dvmAllocArrayByClass;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmAllocArrayByClass");
        callFuncPtr((int)funcPtr, "dvmAllocArrayByClass");
        afterCall("dvmAllocArrayByClass");
    } else {
        callFuncPtr((int)funcPtr, "dvmAllocArrayByClass");
    }
    return 0;
}
//!generate native code to call dvmAllocPrimitiveArray

//!
int call_dvmAllocPrimitiveArray() {
    typedef ArrayObject* (*vmHelper)(char, size_t, int);
    vmHelper funcPtr = dvmAllocPrimitiveArray;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmAllocPrimitiveArray");
        callFuncPtr((int)funcPtr, "dvmAllocPrimitiveArray");
        afterCall("dvmAllocPrimitiveArray");
    } else {
        callFuncPtr((int)funcPtr, "dvmAllocPrimitiveArray");
    }
    return 0;
}
//!generate native code to call dvmInterpHandleFillArrayData

//!
int call_dvmInterpHandleFillArrayData() {
    typedef bool (*vmHelper)(ArrayObject*, const u2*);
    vmHelper funcPtr = dvmInterpHandleFillArrayData;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmInterpHandleFillArrayData"); //before move_imm_to_reg to avoid spilling C_SCRATCH_1
        callFuncPtr((int)funcPtr, "dvmInterpHandleFillArrayData");
        afterCall("dvmInterpHandleFillArrayData");
    } else {
        callFuncPtr((int)funcPtr, "dvmInterpHandleFillArrayData");
    }
    return 0;
}

//!generate native code to call dvmNcgHandlePackedSwitch

//!
int call_dvmNcgHandlePackedSwitch() {
    typedef s4 (*vmHelper)(const s4*, s4, u2, s4);
    vmHelper funcPtr = dvmNcgHandlePackedSwitch;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmNcgHandlePackedSwitch");
        callFuncPtr((int)funcPtr, "dvmNcgHandlePackedSwitch");
        afterCall("dvmNcgHandlePackedSwitch");
    } else {
        callFuncPtr((int)funcPtr, "dvmNcgHandlePackedSwitch");
    }
    return 0;
}

int call_dvmJitHandlePackedSwitch() {
    typedef s4 (*vmHelper)(const s4*, s4, u2, s4);
    vmHelper funcPtr = dvmJitHandlePackedSwitch;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmJitHandlePackedSwitch");
        callFuncPtr((int)funcPtr, "dvmJitHandlePackedSwitch");
        afterCall("dvmJitHandlePackedSwitch");
    } else {
        callFuncPtr((int)funcPtr, "dvmJitHandlePackedSwitch");
    }
    return 0;
}

//!generate native code to call dvmNcgHandleSparseSwitch

//!
int call_dvmNcgHandleSparseSwitch() {
    typedef s4 (*vmHelper)(const s4*, u2, s4);
    vmHelper funcPtr = dvmNcgHandleSparseSwitch;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmNcgHandleSparseSwitch");
        callFuncPtr((int)funcPtr, "dvmNcgHandleSparseSwitch");
        afterCall("dvmNcgHandleSparseSwitch");
    } else {
        callFuncPtr((int)funcPtr, "dvmNcgHandleSparseSwitch");
    }
    return 0;
}

int call_dvmJitHandleSparseSwitch() {
    typedef s4 (*vmHelper)(const s4*, u2, s4);
    vmHelper funcPtr = dvmJitHandleSparseSwitch;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmJitHandleSparseSwitch");
        callFuncPtr((int)funcPtr, "dvmJitHandleSparseSwitch");
        afterCall("dvmJitHandleSparseSwitch");
    } else {
        callFuncPtr((int)funcPtr, "dvmJitHandleSparseSwitch");
    }
    return 0;
}

//!generate native code to call dvmCanPutArrayElement

//!
int call_dvmCanPutArrayElement() {
    typedef bool (*vmHelper)(const ClassObject*, const ClassObject*);
    vmHelper funcPtr = dvmCanPutArrayElement;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmCanPutArrayElement");
        callFuncPtr((int)funcPtr, "dvmCanPutArrayElement");
        afterCall("dvmCanPutArrayElement");
    } else {
        callFuncPtr((int)funcPtr, "dvmCanPutArrayElement");
    }
    return 0;
}

//!generate native code to call dvmFindInterfaceMethodInCache

//!
int call_dvmFindInterfaceMethodInCache() {
    typedef Method* (*vmHelper)(ClassObject*, u4, const Method*, DvmDex*);
    vmHelper funcPtr = dvmFindInterfaceMethodInCache;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmFindInterfaceMethodInCache");
        callFuncPtr((int)funcPtr, "dvmFindInterfaceMethodInCache");
        afterCall("dvmFindInterfaceMethodInCache");
    } else {
        callFuncPtr((int)funcPtr, "dvmFindInterfaceMethodInCache");
    }
    return 0;
}

//!generate native code to call dvmHandleStackOverflow

//!
int call_dvmHandleStackOverflow() {
    typedef void (*vmHelper)(Thread*, const Method*);
    vmHelper funcPtr = dvmHandleStackOverflow;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmHandleStackOverflow");
        callFuncPtr((int)funcPtr, "dvmHandleStackOverflow");
        afterCall("dvmHandleStackOverflow");
    } else {
        callFuncPtr((int)funcPtr, "dvmHandleStackOverflow");
    }
    return 0;
}
//!generate native code to call dvmResolveString

//!
int call_dvmResolveString() {
    //StringObject* dvmResolveString(const ClassObject* referrer, u4 stringIdx)
    typedef StringObject* (*vmHelper)(const ClassObject*, u4);
    vmHelper funcPtr = dvmResolveString;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmResolveString");
        callFuncPtr((int)funcPtr, "dvmResolveString");
        afterCall("dvmResolveString");
    } else {
        callFuncPtr((int)funcPtr, "dvmResolveString");
    }
    return 0;
}
//!generate native code to call dvmResolveInstField

//!
int call_dvmResolveInstField() {
    //InstField* dvmResolveInstField(const ClassObject* referrer, u4 ifieldIdx)
    typedef InstField* (*vmHelper)(const ClassObject*, u4);
    vmHelper funcPtr = dvmResolveInstField;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmResolveInstField");
        callFuncPtr((int)funcPtr, "dvmResolveInstField");
        afterCall("dvmResolveInstField");
    } else {
        callFuncPtr((int)funcPtr, "dvmResolveInstField");
    }
    return 0;
}
//!generate native code to call dvmResolveStaticField

//!
int call_dvmResolveStaticField() {
    //StaticField* dvmResolveStaticField(const ClassObject* referrer, u4 sfieldIdx)
    typedef StaticField* (*vmHelper)(const ClassObject*, u4);
    vmHelper funcPtr = dvmResolveStaticField;
    if(gDvm.executionMode == kExecutionModeNcgO1) {
        beforeCall("dvmResolveStaticField");
        callFuncPtr((int)funcPtr, "dvmResolveStaticField");
        afterCall("dvmResolveStaticField");
    } else {
        callFuncPtr((int)funcPtr, "dvmResolveStaticField");
    }
    return 0;
}

#define P_GPR_2 PhysicalReg_ECX
/*!
\brief This function is used to resolve a string reference

INPUT: const pool index in %eax

OUTPUT: resolved string in %eax

The registers are hard-coded, 2 physical registers %esi and %edx are used as scratch registers;
It calls a C function dvmResolveString;
The only register that is still live after this function is ebx
*/
int const_string_resolve() {
    scratchRegs[0] = PhysicalReg_ESI; scratchRegs[1] = PhysicalReg_EDX;
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;
    insertLabel(".const_string_resolve", false);
    //method stored in glue structure as well as on the interpreted stack
    get_glue_method_class(P_GPR_2, true);
    load_effective_addr(-8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, PhysicalReg_EAX, true, 4, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, P_GPR_2, true, 0, PhysicalReg_ESP, true);
    call_dvmResolveString();
    load_effective_addr(8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    compare_imm_reg( OpndSize_32, 0, PhysicalReg_EAX, true);
    conditional_jump(Condition_E, "common_exceptionThrown", false);
    x86_return();
    return 0;
}
#undef P_GPR_2
/*!
\brief This function is used to resolve a class

INPUT: const pool index in argument "indexReg" (%eax)

OUTPUT: resolved class in %eax

The registers are hard-coded, 3 physical registers (%esi, %edx, startLR:%eax) are used as scratch registers.
It calls a C function dvmResolveClass;
The only register that is still live after this function is ebx
*/
int resolve_class2(
           int startLR/*scratch register*/, bool isPhysical, int indexReg/*const pool index*/,
           bool indexPhysical, int thirdArg) {
    insertLabel(".class_resolve", false);
    scratchRegs[0] = PhysicalReg_ESI; scratchRegs[1] = PhysicalReg_EDX;
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;

    //push index to stack first, to free indexReg
    load_effective_addr(-12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, indexReg, indexPhysical, 4, PhysicalReg_ESP, true);
    get_glue_method_class(startLR, isPhysical);
    move_imm_to_mem(OpndSize_32, thirdArg, 8, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, startLR, isPhysical, 0, PhysicalReg_ESP, true);
    call_dvmResolveClass();
    load_effective_addr(12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true);
    conditional_jump(Condition_E, "common_exceptionThrown", false);

    x86_return();
    return 0;
}
/*!
\brief This function is used to resolve a method, and it is called once with %eax for both indexReg and startLR

INPUT: const pool index in argument "indexReg" (%eax)

OUTPUT: resolved method in %eax

The registers are hard-coded, 3 physical registers (%esi, %edx, startLR:%eax) are used as scratch registers.
It calls a C function dvmResolveMethod;
The only register that is still live after this function is ebx
*/
int resolve_method2(
            int startLR/*logical register index*/, bool isPhysical, int indexReg/*const pool index*/,
            bool indexPhysical,
            int thirdArg/*VIRTUAL*/) {
    if(thirdArg == METHOD_VIRTUAL)
        insertLabel(".virtual_method_resolve", false);
    else if(thirdArg == METHOD_DIRECT)
        insertLabel(".direct_method_resolve", false);
    else if(thirdArg == METHOD_STATIC)
        insertLabel(".static_method_resolve", false);

    load_effective_addr(-12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, indexReg, indexPhysical, 4, PhysicalReg_ESP, true);

    scratchRegs[0] = PhysicalReg_ESI; scratchRegs[1] = PhysicalReg_EDX;
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;
    get_glue_method_class(startLR, isPhysical);

    move_imm_to_mem(OpndSize_32, thirdArg, 8, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, startLR, isPhysical, 0, PhysicalReg_ESP, true);
    call_dvmResolveMethod();
    load_effective_addr(12, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true);
    conditional_jump(Condition_E, "common_exceptionThrown", false);

    x86_return();
    return 0;
}
/*!
\brief This function is used to resolve an instance field

INPUT: const pool index in argument "indexReg" (%eax)

OUTPUT: resolved field in %eax

The registers are hard-coded, 3 physical registers (%esi, %edx, startLR:%eax) are used as scratch registers.
It calls a C function dvmResolveInstField;
The only register that is still live after this function is ebx
*/
int resolve_inst_field2(
            int startLR/*logical register index*/, bool isPhysical,
            int indexReg/*const pool index*/, bool indexPhysical) {
    insertLabel(".inst_field_resolve", false);
    scratchRegs[0] = PhysicalReg_ESI; scratchRegs[1] = PhysicalReg_EDX;
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;

    load_effective_addr(-8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, indexReg, indexPhysical, 4, PhysicalReg_ESP, true);
    //method stored in glue structure as well as interpreted stack
    get_glue_method_class(startLR, isPhysical);
    move_reg_to_mem(OpndSize_32, startLR, isPhysical, 0, PhysicalReg_ESP, true);
    call_dvmResolveInstField();
    load_effective_addr(8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true);
    conditional_jump(Condition_E, "common_exceptionThrown", false);

    x86_return();
    return 0;
}
/*!
\brief This function is used to resolve a static field

INPUT: const pool index in argument "indexReg" (%eax)

OUTPUT: resolved field in %eax

The registers are hard-coded, 3 physical registers (%esi, %edx, startLR:%eax) are used as scratch registers.
It calls a C function dvmResolveStaticField;
The only register that is still live after this function is ebx
*/
int resolve_static_field2(
              int startLR/*logical register index*/, bool isPhysical, int indexReg/*const pool index*/,
              bool indexPhysical) {
    insertLabel(".static_field_resolve", false);
    scratchRegs[0] = PhysicalReg_ESI; scratchRegs[1] = PhysicalReg_EDX;
    scratchRegs[2] = PhysicalReg_Null; scratchRegs[3] = PhysicalReg_Null;

    load_effective_addr(-8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem(OpndSize_32, indexReg, indexPhysical, 4, PhysicalReg_ESP, true);
    get_glue_method_class(startLR, isPhysical);
    move_reg_to_mem(OpndSize_32, startLR, isPhysical, 0, PhysicalReg_ESP, true);
    call_dvmResolveStaticField();
    load_effective_addr(8, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    compare_imm_reg(OpndSize_32, 0, PhysicalReg_EAX, true);
    conditional_jump(Condition_E, "common_exceptionThrown", false);

    x86_return();
    return 0;
}

int pushAllRegs() {
    load_effective_addr(-28, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    move_reg_to_mem_noalloc(OpndSize_32, PhysicalReg_EAX, true, 24, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1);
    move_reg_to_mem_noalloc(OpndSize_32, PhysicalReg_EBX, true, 20, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1);
    move_reg_to_mem_noalloc(OpndSize_32, PhysicalReg_ECX, true, 16, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1);
    move_reg_to_mem_noalloc(OpndSize_32, PhysicalReg_EDX, true, 12, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1);
    move_reg_to_mem_noalloc(OpndSize_32, PhysicalReg_ESI, true, 8, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1);
    move_reg_to_mem_noalloc(OpndSize_32, PhysicalReg_EDI, true, 4, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1);
    move_reg_to_mem_noalloc(OpndSize_32, PhysicalReg_EBP, true, 0, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1);
    return 0;
}
int popAllRegs() {
    move_mem_to_reg_noalloc(OpndSize_32, 24, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1, PhysicalReg_EAX, true);
    move_mem_to_reg_noalloc(OpndSize_32, 20, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1, PhysicalReg_EBX, true);
    move_mem_to_reg_noalloc(OpndSize_32, 16, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1, PhysicalReg_ECX, true);
    move_mem_to_reg_noalloc(OpndSize_32, 12, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1, PhysicalReg_EDX, true);
    move_mem_to_reg_noalloc(OpndSize_32, 8, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1, PhysicalReg_ESI, true);
    move_mem_to_reg_noalloc(OpndSize_32, 4, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1, PhysicalReg_EDI, true);
    move_mem_to_reg_noalloc(OpndSize_32, 0, PhysicalReg_ESP, true, MemoryAccess_Unknown, -1, PhysicalReg_EBP, true);
    load_effective_addr(28, PhysicalReg_ESP, true, PhysicalReg_ESP, true);
    return 0;
}

void dump_nop(int size) {
    switch(size) {
        case 1:
          *stream = 0x90;
          break;
        case 2:
          *stream = 0x66;
          *(stream +1) = 0x90;
          break;
        case 3:
          *stream = 0x0f;
          *(stream + 1) = 0x1f;
          *(stream + 2) = 0x00;
          break;
        default:
          //TODO: add more cases.
          break;
    }
    stream += size;
}
