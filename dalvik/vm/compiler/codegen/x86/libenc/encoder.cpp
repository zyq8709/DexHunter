/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/**
 * @author Alexander V. Astapchuk
 */
#include <stdio.h>
#include <assert.h>

#include <limits.h>

#include "enc_base.h"

#ifdef NO_ENCODER_INLINE
    #include "encoder.h"
    #include "encoder.inl"
#else
    #define NO_ENCODER_INLINE
    #include "encoder.h"
    #undef NO_ENCODER_INLINE
#endif



#ifdef _EM64T_

R_Opnd rax_opnd(rax_reg);
R_Opnd rcx_opnd(rcx_reg);
R_Opnd rdx_opnd(rdx_reg);
R_Opnd rbx_opnd(rbx_reg);
R_Opnd rsp_opnd(rsp_reg);
R_Opnd rbp_opnd(rbp_reg);
R_Opnd rsi_opnd(rsi_reg);
R_Opnd rdi_opnd(rdi_reg);

R_Opnd r8_opnd(r8_reg);
R_Opnd r9_opnd(r9_reg);
R_Opnd r10_opnd(r10_reg);
R_Opnd r11_opnd(r11_reg);
R_Opnd r12_opnd(r12_reg);
R_Opnd r13_opnd(r13_reg);
R_Opnd r14_opnd(r14_reg);
R_Opnd r15_opnd(r15_reg);

XMM_Opnd xmm8_opnd(xmm8_reg);
XMM_Opnd xmm9_opnd(xmm9_reg);
XMM_Opnd xmm10_opnd(xmm10_reg);
XMM_Opnd xmm11_opnd(xmm11_reg);
XMM_Opnd xmm12_opnd(xmm12_reg);
XMM_Opnd xmm13_opnd(xmm13_reg);
XMM_Opnd xmm14_opnd(xmm14_reg);
XMM_Opnd xmm15_opnd(xmm15_reg);

#else

R_Opnd eax_opnd(eax_reg);
R_Opnd ecx_opnd(ecx_reg);
R_Opnd edx_opnd(edx_reg);
R_Opnd ebx_opnd(ebx_reg);
R_Opnd esp_opnd(esp_reg);
R_Opnd ebp_opnd(ebp_reg);
R_Opnd esi_opnd(esi_reg);
R_Opnd edi_opnd(edi_reg);

#endif //_EM64T_

XMM_Opnd xmm0_opnd(xmm0_reg);
XMM_Opnd xmm1_opnd(xmm1_reg);
XMM_Opnd xmm2_opnd(xmm2_reg);
XMM_Opnd xmm3_opnd(xmm3_reg);
XMM_Opnd xmm4_opnd(xmm4_reg);
XMM_Opnd xmm5_opnd(xmm5_reg);
XMM_Opnd xmm6_opnd(xmm6_reg);
XMM_Opnd xmm7_opnd(xmm7_reg);


#define countof(a)      (sizeof(a)/sizeof(a[0]))

extern const RegName map_of_regno_2_regname[];
extern const OpndSize map_of_EncoderOpndSize_2_RealOpndSize[];
extern const Mnemonic map_of_alu_opcode_2_mnemonic[];
extern const Mnemonic map_of_shift_opcode_2_mnemonic[];

const RegName map_of_regno_2_regname [] = {
#ifdef _EM64T_
    RegName_RAX,    RegName_RBX,    RegName_RCX,    RegName_RDX,
    RegName_RDI,    RegName_RSI,    RegName_RSP,    RegName_RBP,
    RegName_R8,     RegName_R9,     RegName_R10,    RegName_R11,
    RegName_R12,    RegName_R13,    RegName_R14,    RegName_R15,
    RegName_XMM0,   RegName_XMM1,   RegName_XMM2,   RegName_XMM3,
    RegName_XMM4,   RegName_XMM5,   RegName_XMM6,   RegName_XMM7,
    RegName_XMM8,   RegName_XMM9,   RegName_XMM10,  RegName_XMM11,
    RegName_XMM12,  RegName_XMM13,   RegName_XMM14, RegName_XMM15,

#else
    RegName_EAX,    RegName_EBX,    RegName_ECX,    RegName_EDX,
    RegName_EDI,    RegName_ESI,    RegName_ESP,    RegName_EBP,
    RegName_XMM0,   RegName_XMM1,   RegName_XMM2,   RegName_XMM3,
    RegName_XMM4,   RegName_XMM5,   RegName_XMM6,   RegName_XMM7,
    RegName_FS,
#endif  // _EM64T_

    RegName_Null,
};

const OpndSize map_of_EncoderOpndSize_2_RealOpndSize[] = {
    OpndSize_8, OpndSize_16, OpndSize_32, OpndSize_64, OpndSize_Any
};

const Mnemonic map_of_alu_opcode_2_mnemonic[] = {
    //add_opc=0,  or_opc,           adc_opc,        sbb_opc,
    //and_opc,      sub_opc,        xor_opc,        cmp_opc,
    //n_alu
    Mnemonic_ADD,   Mnemonic_OR,    Mnemonic_ADC,   Mnemonic_SBB,
    Mnemonic_AND,   Mnemonic_SUB,   Mnemonic_XOR,   Mnemonic_CMP,
};

const Mnemonic map_of_shift_opcode_2_mnemonic[] = {
    //shld_opc, shrd_opc,
    // shl_opc, shr_opc, sar_opc, ror_opc, max_shift_opcode=6,
    // n_shift = 6
    Mnemonic_SHLD,  Mnemonic_SHRD,
    Mnemonic_SHL,   Mnemonic_SHR,   Mnemonic_SAR, Mnemonic_ROR
};

#ifdef _DEBUG

static int debug_check() {
    // Checks some assumptions.

    // 1. all items of Encoder.h:enum Reg_No  must be mapped plus n_reg->RegName_Null
    assert(countof(map_of_regno_2_regname) == n_reg + 1);
    assert(countof(map_of_alu_opcode_2_mnemonic) == n_alu);
    assert(countof(map_of_shift_opcode_2_mnemonic) == n_shift);
    return 0;
}

static int dummy = debug_check();

// can have this - initialization order problems.... static int dummy_run_the_debug_test = debug_check();

#endif
