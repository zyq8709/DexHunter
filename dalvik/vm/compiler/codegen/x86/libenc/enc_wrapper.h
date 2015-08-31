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

#ifndef _VM_ENC_WRAPPER_H_
#define _VM_ENC_WRAPPER_H_

#include "enc_defs_ext.h"

extern bool dump_x86_inst;
typedef enum PhysicalReg {
  PhysicalReg_EAX = 0, PhysicalReg_EBX, PhysicalReg_ECX, PhysicalReg_EDX,
  PhysicalReg_EDI, PhysicalReg_ESI, PhysicalReg_ESP, PhysicalReg_EBP,
  PhysicalReg_XMM0, PhysicalReg_XMM1, PhysicalReg_XMM2, PhysicalReg_XMM3,
  PhysicalReg_XMM4, PhysicalReg_XMM5, PhysicalReg_XMM6, PhysicalReg_XMM7,
  PhysicalReg_ST0,  PhysicalReg_ST1, PhysicalReg_ST2,  PhysicalReg_ST3,
  PhysicalReg_ST4, PhysicalReg_ST5, PhysicalReg_ST6, PhysicalReg_ST7,
  PhysicalReg_Null,
  //used as scratch logical register in NCG O1
  //should not overlap with regular logical register, start from 100
  PhysicalReg_SCRATCH_1 = 100, PhysicalReg_SCRATCH_2, PhysicalReg_SCRATCH_3, PhysicalReg_SCRATCH_4,
  PhysicalReg_SCRATCH_5, PhysicalReg_SCRATCH_6, PhysicalReg_SCRATCH_7, PhysicalReg_SCRATCH_8,
  PhysicalReg_SCRATCH_9, PhysicalReg_SCRATCH_10,
  PhysicalReg_GLUE_DVMDEX = 900,
  PhysicalReg_GLUE = 901
} PhysicalReg;

typedef enum Reg_No {
#ifdef _EM64T_
    rax_reg = 0,rbx_reg,    rcx_reg,    rdx_reg,
    rdi_reg,    rsi_reg,    rsp_reg,    rbp_reg,
    r8_reg,     r9_reg,     r10_reg,    r11_reg,
    r12_reg,    r13_reg,    r14_reg,    r15_reg,
    xmm0_reg,   xmm1_reg,   xmm2_reg,   xmm3_reg,
    xmm4_reg,   xmm5_reg,   xmm6_reg,   xmm7_reg,
    xmm8_reg,   xmm9_reg,   xmm10_reg,  xmm11_reg,
    xmm12_reg,  xmm13_reg,  xmm14_reg,  xmm15_reg,

#else   // !defined(_EM64T_)

    eax_reg = 0,ebx_reg,    ecx_reg,    edx_reg,
    edi_reg,    esi_reg,    esp_reg,    ebp_reg,
    xmm0_reg,   xmm1_reg,   xmm2_reg,   xmm3_reg,
    xmm4_reg,   xmm5_reg,   xmm6_reg,   xmm7_reg,
    fs_reg,
#endif
    /** @brief Total number of registers.*/
    n_reg
} Reg_No;
//
// instruction operand sizes: 8,16,32,64 bits
//
typedef enum Opnd_Size {
    size_8 = 0,
    size_16,
    size_32,
    size_64,
    n_size,
#ifdef _EM64T_
    size_platf = size_64
#else
    size_platf = size_32
#endif
} Opnd_Size;

//
// opcodes for alu instructions
//
typedef enum ALU_Opcode {
    add_opc = 0,or_opc,     adc_opc,    sbb_opc,
    and_opc,    sub_opc,    xor_opc,    cmp_opc,
    mul_opc,    imul_opc,   div_opc,    idiv_opc,
    sll_opc,    srl_opc,    sra_opc, //shift right arithmetic
    shl_opc,    shr_opc,
    sal_opc,    sar_opc,
    neg_opc,    not_opc,    andn_opc,
    n_alu
} ALU_Opcode;

typedef enum ConditionCode {
    Condition_O     = 0,
    Condition_NO    = 1,
    Condition_B     = 2,
    Condition_NAE   = Condition_B,
    Condition_C     = Condition_B,
    Condition_NB    = 3,
    Condition_AE    = Condition_NB,
    Condition_NC    = Condition_NB,
    Condition_Z     = 4,
    Condition_E     = Condition_Z,
    Condition_NZ    = 5,
    Condition_NE    = Condition_NZ,
    Condition_BE    = 6,
    Condition_NA    = Condition_BE,
    Condition_NBE   = 7,
    Condition_A     = Condition_NBE,

    Condition_S     = 8,
    Condition_NS    = 9,
    Condition_P     = 10,
    Condition_PE    = Condition_P,
    Condition_NP    = 11,
    Condition_PO    = Condition_NP,
    Condition_L     = 12,
    Condition_NGE   = Condition_L,
    Condition_NL    = 13,
    Condition_GE    = Condition_NL,
    Condition_LE    = 14,
    Condition_NG    = Condition_LE,
    Condition_NLE   = 15,
    Condition_G     = Condition_NLE,
    Condition_Count = 16
} ConditionCode;

//
// prefix code
//
typedef enum InstrPrefix {
    no_prefix,
    lock_prefix                     = 0xF0,
    hint_branch_taken_prefix        = 0x2E,
    hint_branch_not_taken_prefix    = 0x3E,
    prefix_repne                    = 0xF2,
    prefix_repnz                    = prefix_repne,
    prefix_repe                     = 0xF3,
    prefix_repz                     = prefix_repe,
    prefix_rep                      = 0xF3,
    prefix_cs                       = 0x2E,
    prefix_ss                       = 0x36,
    prefix_ds                       = 0x3E,
    prefix_es                       = 0x26,
    prefix_fs                       = 0x64,
    prefix_gs                       = 0x65
} InstrPrefix;

//last 2 bits: decide xmm, gp, fs
//virtual, scratch, temp, hard match with ncg_o1_data.h
typedef enum LowOpndRegType {
  LowOpndRegType_gp = 0,
  LowOpndRegType_fs = 1,
  LowOpndRegType_xmm = 2,
  LowOpndRegType_fs_s = 3,
  LowOpndRegType_ss = 4,
  LowOpndRegType_scratch = 8, //used by NCG O1
  LowOpndRegType_temp = 16,
  LowOpndRegType_hard = 32,   //NCG O1
  LowOpndRegType_virtual = 64, //used by NCG O1
  LowOpndRegType_glue = 128
} LowOpndRegType;

//if inline, separte enc_wrapper.cpp into two files, one of them is .inl
//           enc_wrapper.cpp needs to handle both cases
#ifdef ENCODER_INLINE
    #define ENCODER_DECLARE_EXPORT inline
    #include "enc_wrapper.inl"
#else
    #define ENCODER_DECLARE_EXPORT
#endif

#ifdef __cplusplus
extern "C"
{
#endif
ENCODER_DECLARE_EXPORT char* encoder_imm(Mnemonic m, OpndSize size,
                  int imm, char* stream);
ENCODER_DECLARE_EXPORT unsigned encoder_get_inst_size(char * stream);
ENCODER_DECLARE_EXPORT char* encoder_update_imm(int imm, char * stream);
ENCODER_DECLARE_EXPORT char* encoder_mem(Mnemonic m, OpndSize size,
               int disp, int base_reg, bool isBasePhysical, char* stream);
ENCODER_DECLARE_EXPORT char* encoder_reg(Mnemonic m, OpndSize size,
               int reg, bool isPhysical, LowOpndRegType type, char* stream);
ENCODER_DECLARE_EXPORT char* encoder_reg_reg(Mnemonic m, OpndSize size,
                   int reg, bool isPhysical,
                   int reg2, bool isPhysical2, LowOpndRegType type, char* stream);
ENCODER_DECLARE_EXPORT char* encoder_mem_reg(Mnemonic m, OpndSize size,
                   int disp, int base_reg, bool isBasePhysical,
                   int reg, bool isPhysical, LowOpndRegType type, char* stream);
ENCODER_DECLARE_EXPORT char* encoder_mem_scale_reg(Mnemonic m, OpndSize size,
                         int base_reg, bool isBasePhysical, int index_reg, bool isIndexPhysical, int scale,
                         int reg, bool isPhysical, LowOpndRegType type, char* stream);
ENCODER_DECLARE_EXPORT char* encoder_reg_mem_scale(Mnemonic m, OpndSize size,
                         int reg, bool isPhysical,
                         int base_reg, bool isBasePhysical, int index_reg, bool isIndexPhysical, int scale,
                         LowOpndRegType type, char* stream);
ENCODER_DECLARE_EXPORT char * encoder_mem_disp_scale_reg(Mnemonic m, OpndSize size,
                         int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale,
                         int reg, bool isPhysical, LowOpndRegType type, char * stream);
ENCODER_DECLARE_EXPORT char * encoder_movzs_mem_disp_scale_reg(Mnemonic m, OpndSize size,
                         int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale,
                         int reg, bool isPhysical, LowOpndRegType type, char * stream);
ENCODER_DECLARE_EXPORT char* encoder_reg_mem_disp_scale(Mnemonic m, OpndSize size,
                         int reg, bool isPhysical,
                         int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale,
                         LowOpndRegType type, char* stream);
ENCODER_DECLARE_EXPORT char* encoder_reg_mem(Mnemonic m, OpndSize size,
                   int reg, bool isPhysical,
                   int disp, int base_reg, bool isBasePhysical, LowOpndRegType type, char* stream);
ENCODER_DECLARE_EXPORT char* encoder_imm_reg(Mnemonic m, OpndSize size,
                   int imm, int reg, bool isPhysical, LowOpndRegType type, char* stream);
ENCODER_DECLARE_EXPORT char * encoder_update_imm_rm(int imm, char * stream);
ENCODER_DECLARE_EXPORT char* encoder_imm_mem(Mnemonic m, OpndSize size,
                   int imm,
                   int disp, int base_reg, bool isBasePhysical, char* stream);
ENCODER_DECLARE_EXPORT char* encoder_fp_mem(Mnemonic m, OpndSize size, int reg,
                  int disp, int base_reg, bool isBasePhysical, char* stream);
ENCODER_DECLARE_EXPORT char* encoder_mem_fp(Mnemonic m, OpndSize size,
                  int disp, int base_reg, bool isBasePhysical,
                  int reg, char* stream);
ENCODER_DECLARE_EXPORT char* encoder_return(char* stream);
ENCODER_DECLARE_EXPORT char* encoder_compare_fp_stack(bool pop, int reg, bool isDouble, char* stream);
ENCODER_DECLARE_EXPORT char* encoder_movez_mem_to_reg(OpndSize size,
                      int disp, int base_reg, bool isBasePhysical,
                      int reg, bool isPhysical, char* stream);
ENCODER_DECLARE_EXPORT char* encoder_moves_mem_to_reg(OpndSize size,
                      int disp, int base_reg, bool isBasePhysical,
                      int reg, bool isPhysical, char* stream);
ENCODER_DECLARE_EXPORT char * encoder_movez_reg_to_reg(OpndSize size,
                      int reg, bool isPhysical, int reg2,
                      bool isPhysical2, LowOpndRegType type, char * stream);
ENCODER_DECLARE_EXPORT char * encoder_moves_reg_to_reg(OpndSize size,
                      int reg, bool isPhysical, int reg2,
                      bool isPhysical2, LowOpndRegType type, char * stream);
ENCODER_DECLARE_EXPORT int decodeThenPrint(char* stream_start);
ENCODER_DECLARE_EXPORT char* decoder_disassemble_instr(char* stream, char* strbuf, unsigned int len);
#ifdef __cplusplus
}
#endif
#endif // _VM_ENC_WRAPPER_H_
