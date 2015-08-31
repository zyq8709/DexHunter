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

#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include "cutils/log.h"
#include "enc_base.h"
#include "enc_wrapper.h"
#include "dec_base.h"

//#define PRINT_ENCODER_STREAM
bool dump_x86_inst = false;
//map_reg
const RegName map_of_regno_2_regname[] = {
    RegName_EAX,    RegName_EBX,    RegName_ECX,    RegName_EDX,
    RegName_EDI,    RegName_ESI,    RegName_ESP,    RegName_EBP,
    RegName_XMM0,   RegName_XMM1,   RegName_XMM2,   RegName_XMM3,
    RegName_XMM4,   RegName_XMM5,   RegName_XMM6,   RegName_XMM7,
    RegName_Null,   RegName_Null,   RegName_Null,   RegName_Null,
    RegName_Null,   RegName_Null,   RegName_Null,   RegName_Null,
    RegName_Null,
    RegName_Null,   RegName_Null,   RegName_Null,   RegName_Null,   RegName_Null,
    RegName_Null,   RegName_Null,   RegName_Null,   RegName_Null,
    RegName_Null,   RegName_Null,   RegName_Null,   RegName_Null,
    RegName_Null,   RegName_Null,   //SCRATCH
    RegName_Null,   RegName_Null,   RegName_Null,   RegName_Null
};

//getRegSize, getAliasReg:
//OpndSize, RegName, OpndExt: enum enc_defs.h
inline void add_r(EncoderBase::Operands & args, int physicalReg, OpndSize sz, OpndExt ext = OpndExt_None) {
    RegName reg = map_of_regno_2_regname[physicalReg];
    if (sz != getRegSize(reg)) {
       reg = getAliasReg(reg, sz);
    }
    args.add(EncoderBase::Operand(reg, ext));
}
inline void add_m(EncoderBase::Operands & args, int baseReg, int disp, OpndSize sz, OpndExt ext = OpndExt_None) {
    args.add(EncoderBase::Operand(sz,
                                  map_of_regno_2_regname[baseReg],
                                  RegName_Null, 0,
                                  disp, ext));
}
inline void add_m_scale(EncoderBase::Operands & args, int baseReg, int indexReg, int scale,
                        OpndSize sz, OpndExt ext = OpndExt_None) {
    args.add(EncoderBase::Operand(sz,
                                  map_of_regno_2_regname[baseReg],
                                  map_of_regno_2_regname[indexReg], scale,
                                  0, ext));
}
inline void add_m_disp_scale(EncoderBase::Operands & args, int baseReg, int disp, int indexReg, int scale,
                        OpndSize sz, OpndExt ext = OpndExt_None) {
    args.add(EncoderBase::Operand(sz,
                                  map_of_regno_2_regname[baseReg],
                                  map_of_regno_2_regname[indexReg], scale,
                                  disp, ext));
}

inline void add_fp(EncoderBase::Operands & args, unsigned i, bool dbl) {
    return args.add((RegName)( (dbl ? RegName_FP0D : RegName_FP0S) + i));
}
inline void add_imm(EncoderBase::Operands & args, OpndSize sz, int value, bool is_signed) {
    //assert(n_size != imm.get_size());
    args.add(EncoderBase::Operand(sz, value,
             is_signed ? OpndExt_Signed : OpndExt_Zero));
}

#define MAX_DECODED_STRING_LEN 1024
char tmpBuffer[MAX_DECODED_STRING_LEN];

void printOperand(const EncoderBase::Operand & opnd) {
    unsigned int sz;
    if(!dump_x86_inst) return;
    sz = strlen(tmpBuffer);
    if(opnd.size() != OpndSize_32) {
        sz += snprintf(&tmpBuffer[sz], MAX_DECODED_STRING_LEN-sz, "%s ",
                       getOpndSizeString(opnd.size()));
    }
    if(opnd.is_mem()) {
        if(opnd.scale() != 0) {
            sz += snprintf(&tmpBuffer[sz], MAX_DECODED_STRING_LEN-sz,
                           "%d(%s,%s,%d)", opnd.disp(),
                           getRegNameString(opnd.base()),
                           getRegNameString(opnd.index()), opnd.scale());
        } else {
            sz += snprintf(&tmpBuffer[sz], MAX_DECODED_STRING_LEN-sz, "%d(%s)",
                           opnd.disp(), getRegNameString(opnd.base()));
        }
    }
    if(opnd.is_imm()) {
        sz += snprintf(&tmpBuffer[sz], MAX_DECODED_STRING_LEN-sz, "#%x",
                       (int)opnd.imm());
    }
    if(opnd.is_reg()) {
        sz += snprintf(&tmpBuffer[sz], MAX_DECODED_STRING_LEN-sz, "%s",
                       getRegNameString(opnd.reg()));
    }
}
//TODO: the order of operands
//to make the printout have the same order as assembly in .S
//I reverse the order here
void printDecoderInst(Inst & decInst) {
    unsigned int sz;
    if(!dump_x86_inst) return;
    sz = strlen(tmpBuffer);
    sz += snprintf(&tmpBuffer[sz], MAX_DECODED_STRING_LEN-sz, "%s ",
                   EncoderBase::toStr(decInst.mn));
    for(unsigned int k = 0; k < decInst.argc; k++) {
        if(k > 0) {
            sz = strlen(tmpBuffer);
            sz += snprintf(&tmpBuffer[sz], MAX_DECODED_STRING_LEN-sz, ", ");
        }
        printOperand(decInst.operands[decInst.argc-1-k]);
    }
    ALOGE("%s", tmpBuffer);
}
void printOperands(EncoderBase::Operands& opnds) {
    unsigned int sz;
    if(!dump_x86_inst) return;
    for(unsigned int k = 0; k < opnds.count(); k++) {
        if(k > 0) {
            sz = strlen(tmpBuffer);
            sz += snprintf(&tmpBuffer[sz], MAX_DECODED_STRING_LEN-sz, ", ");
        }
        printOperand(opnds[opnds.count()-1-k]);
    }
}
void printEncoderInst(Mnemonic m, EncoderBase::Operands& opnds) {
    if(!dump_x86_inst) return;
    snprintf(tmpBuffer, MAX_DECODED_STRING_LEN, "--- ENC %s ",
             EncoderBase::toStr(m));
    printOperands(opnds);
    ALOGE("%s", tmpBuffer);
}
int decodeThenPrint(char* stream_start) {
    if(!dump_x86_inst) return 0;
    snprintf(tmpBuffer, MAX_DECODED_STRING_LEN, "--- INST @ %p: ",
             stream_start);
    Inst decInst;
    unsigned numBytes = DecoderBase::decode(stream_start, &decInst);
    printDecoderInst(decInst);
    return numBytes;
}

extern "C" ENCODER_DECLARE_EXPORT char * encoder_imm(Mnemonic m, OpndSize size, int imm, char * stream) {
    EncoderBase::Operands args;
    //assert(imm.get_size() == size_32);
    add_imm(args, size, imm, true/*is_signed*/);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT unsigned encoder_get_inst_size(char * stream) {
    Inst decInst;
    unsigned numBytes = DecoderBase::decode(stream, &decInst);
    return numBytes;
}

extern "C" ENCODER_DECLARE_EXPORT unsigned encoder_get_cur_operand_offset(int opnd_id)
{
    return (unsigned)EncoderBase::getOpndLocation(opnd_id);
}

extern "C" ENCODER_DECLARE_EXPORT char * encoder_update_imm(int imm, char * stream) {
    Inst decInst;
    unsigned numBytes = DecoderBase::decode(stream, &decInst);
    EncoderBase::Operands args;
    //assert(imm.get_size() == size_32);
    add_imm(args, decInst.operands[0].size(), imm, true/*is_signed*/);
    char* stream_next = (char *)EncoderBase::encode(stream, decInst.mn, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(decInst.mn, args);
    decodeThenPrint(stream);
#endif
    return stream_next;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_mem(Mnemonic m, OpndSize size,
               int disp, int base_reg, bool isBasePhysical, char * stream) {
    EncoderBase::Operands args;
    add_m(args, base_reg, disp, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_reg(Mnemonic m, OpndSize size,
               int reg, bool isPhysical, LowOpndRegType type, char * stream) {
    EncoderBase::Operands args;
    if(m == Mnemonic_IDIV || m == Mnemonic_MUL || m == Mnemonic_IMUL) {
      add_r(args, 0/*eax*/, size);
      add_r(args, 3/*edx*/, size);
    }
    add_r(args, reg, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
//both operands have same size
extern "C" ENCODER_DECLARE_EXPORT char * encoder_reg_reg(Mnemonic m, OpndSize size,
                   int reg, bool isPhysical,
                   int reg2, bool isPhysical2, LowOpndRegType type, char * stream) {
    if((m == Mnemonic_MOV || m == Mnemonic_MOVQ) && reg == reg2) return stream;
    EncoderBase::Operands args;
    add_r(args, reg2, size); //destination
    if(m == Mnemonic_SAL || m == Mnemonic_SHR || m == Mnemonic_SHL || m == Mnemonic_SAR)
      add_r(args, reg, OpndSize_8);
    else
      add_r(args, reg, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_mem_reg(Mnemonic m, OpndSize size,
                   int disp, int base_reg, bool isBasePhysical,
                   int reg, bool isPhysical, LowOpndRegType type, char * stream) {
    EncoderBase::Operands args;
    add_r(args, reg, size);
    add_m(args, base_reg, disp, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_mem_scale_reg(Mnemonic m, OpndSize size,
                         int base_reg, bool isBasePhysical, int index_reg, bool isIndexPhysical, int scale,
                         int reg, bool isPhysical, LowOpndRegType type, char * stream) {
    EncoderBase::Operands args;
    add_r(args, reg, size);
    add_m_scale(args, base_reg, index_reg, scale, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_reg_mem_scale(Mnemonic m, OpndSize size,
                         int reg, bool isPhysical,
                         int base_reg, bool isBasePhysical, int index_reg, bool isIndexPhysical, int scale,
                         LowOpndRegType type, char * stream) {
    EncoderBase::Operands args;
    add_m_scale(args, base_reg, index_reg, scale, size);
    add_r(args, reg, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_mem_disp_scale_reg(Mnemonic m, OpndSize size,
                         int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale,
                         int reg, bool isPhysical, LowOpndRegType type, char * stream) {
    EncoderBase::Operands args;
    add_r(args, reg, size);
    add_m_disp_scale(args, base_reg, disp, index_reg, scale, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_movzs_mem_disp_scale_reg(Mnemonic m, OpndSize size,
                         int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale,
                         int reg, bool isPhysical, LowOpndRegType type, char * stream) {
    EncoderBase::Operands args;
    add_r(args, reg, OpndSize_32);
    add_m_disp_scale(args, base_reg, disp, index_reg, scale, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}

extern "C" ENCODER_DECLARE_EXPORT char* encoder_reg_mem_disp_scale(Mnemonic m, OpndSize size,
                         int reg, bool isPhysical,
                         int base_reg, bool isBasePhysical, int disp, int index_reg, bool isIndexPhysical, int scale,
                         LowOpndRegType type, char* stream) {
    EncoderBase::Operands args;
    add_m_disp_scale(args, base_reg, disp, index_reg, scale, size);
    add_r(args, reg, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}

extern "C" ENCODER_DECLARE_EXPORT char * encoder_reg_mem(Mnemonic m, OpndSize size,
                   int reg, bool isPhysical,
                   int disp, int base_reg, bool isBasePhysical, LowOpndRegType type, char * stream) {
    EncoderBase::Operands args;
    add_m(args, base_reg, disp, size);
    add_r(args, reg, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_imm_reg(Mnemonic m, OpndSize size,
                   int imm, int reg, bool isPhysical, LowOpndRegType type, char * stream) {
    EncoderBase::Operands args;
    add_r(args, reg, size); //dst
    if(m == Mnemonic_IMUL) add_r(args, reg, size); //src CHECK
    if(m == Mnemonic_SAL || m == Mnemonic_SHR || m == Mnemonic_SHL
       || m == Mnemonic_SAR || m == Mnemonic_ROR)  //fix for shift opcodes
      add_imm(args, OpndSize_8, imm, true/*is_signed*/);
    else
      add_imm(args, size, imm, true/*is_signed*/);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_update_imm_rm(int imm, char * stream) {
    Inst decInst;
    unsigned numBytes = DecoderBase::decode(stream, &decInst);
    EncoderBase::Operands args;
    args.add(decInst.operands[0]);
    add_imm(args, decInst.operands[1].size(), imm, true/*is_signed*/);
    char* stream_next = (char *)EncoderBase::encode(stream, decInst.mn, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(decInst.mn, args);
    decodeThenPrint(stream);
#endif
    return stream_next;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_imm_mem(Mnemonic m, OpndSize size,
                   int imm,
                   int disp, int base_reg, bool isBasePhysical, char * stream) {
    EncoderBase::Operands args;
    add_m(args, base_reg, disp, size);
    if (m == Mnemonic_SAL || m == Mnemonic_SHR || m == Mnemonic_SHL
        || m == Mnemonic_SAR || m == Mnemonic_ROR)
        size = OpndSize_8;
    add_imm(args, size, imm, true);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_fp_mem(Mnemonic m, OpndSize size, int reg,
                  int disp, int base_reg, bool isBasePhysical, char * stream) {
    EncoderBase::Operands args;
    add_m(args, base_reg, disp, size);
    // a fake FP register as operand
    add_fp(args, reg, size == OpndSize_64/*is_double*/);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_mem_fp(Mnemonic m, OpndSize size,
                  int disp, int base_reg, bool isBasePhysical,
                  int reg, char * stream) {
    EncoderBase::Operands args;
    // a fake FP register as operand
    add_fp(args, reg, size == OpndSize_64/*is_double*/);
    add_m(args, base_reg, disp, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}

extern "C" ENCODER_DECLARE_EXPORT char * encoder_return(char * stream) {
    EncoderBase::Operands args;
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, Mnemonic_RET, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(Mnemonic_RET, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_compare_fp_stack(bool pop, int reg, bool isDouble, char * stream) {
    //Mnemonic m = pop ? Mnemonic_FUCOMP : Mnemonic_FUCOM;
    Mnemonic m = pop ? Mnemonic_FUCOMIP : Mnemonic_FUCOMI;
    //a single operand or 2 operands?
    //FST ST(i) has a single operand in encoder.inl?
    EncoderBase::Operands args;
    add_fp(args, reg, isDouble);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, m, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(m, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_movez_mem_to_reg(OpndSize size,
                      int disp, int base_reg, bool isBasePhysical,
                      int reg, bool isPhysical, char * stream) {
    EncoderBase::Operands args;
    add_r(args, reg, OpndSize_32);
    add_m(args, base_reg, disp, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, Mnemonic_MOVZX, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(Mnemonic_MOVZX, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_moves_mem_to_reg(OpndSize size,
                      int disp, int base_reg, bool isBasePhysical,
                      int reg, bool isPhysical, char * stream) {
    EncoderBase::Operands args;
    add_r(args, reg, OpndSize_32);
    add_m(args, base_reg, disp, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, Mnemonic_MOVSX, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(Mnemonic_MOVSX, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_movez_reg_to_reg(OpndSize size,
                      int reg, bool isPhysical, int reg2,
                      bool isPhysical2, LowOpndRegType type, char * stream) {
    EncoderBase::Operands args;
    add_r(args, reg2, OpndSize_32); //destination
    add_r(args, reg, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, Mnemonic_MOVZX, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(Mnemonic_MOVZX, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}
extern "C" ENCODER_DECLARE_EXPORT char * encoder_moves_reg_to_reg(OpndSize size,
                      int reg, bool isPhysical,int reg2,
                      bool isPhysical2, LowOpndRegType type, char * stream) {
    EncoderBase::Operands args;
    add_r(args, reg2, OpndSize_32); //destination
    add_r(args, reg, size);
    char* stream_start = stream;
    stream = (char *)EncoderBase::encode(stream, Mnemonic_MOVSX, args);
#ifdef PRINT_ENCODER_STREAM
    printEncoderInst(Mnemonic_MOVSX, args);
    decodeThenPrint(stream_start);
#endif
    return stream;
}

// Disassemble the operand "opnd" and put the readable format in "strbuf"
// up to a string length of "len".
unsigned int DisassembleOperandToBuf(const EncoderBase::Operand& opnd, char* strbuf, unsigned int len)
{
    unsigned int sz = 0;
    if(opnd.size() != OpndSize_32) {
        sz += snprintf(&strbuf[sz], len-sz, "%s ",
                       getOpndSizeString(opnd.size()));
    }
    if(opnd.is_mem()) {
        if(opnd.scale() != 0) {
            sz += snprintf(&strbuf[sz], len-sz, "%d(%s,%s,%d)", opnd.disp(),
                           getRegNameString(opnd.base()),
                           getRegNameString(opnd.index()), opnd.scale());
        } else {
            sz += snprintf(&strbuf[sz], len-sz, "%d(%s)",
                           opnd.disp(), getRegNameString(opnd.base()));
        }
    } else if(opnd.is_imm()) {
        sz += snprintf(&strbuf[sz], len-sz, "#%x", (int)opnd.imm());
    } else if(opnd.is_reg()) {
        sz += snprintf(&strbuf[sz], len-sz, "%s",
                       getRegNameString(opnd.reg()));
    }
    return sz;
}

// Disassemble the instruction "decInst" and put the readable format
// in "strbuf" up to a string length of "len".
void DisassembleInstToBuf(Inst& decInst, char* strbuf, unsigned int len)
{
    unsigned int sz = 0;
    int k;
    sz += snprintf(&strbuf[sz], len-sz, "%s ", EncoderBase::toStr(decInst.mn));
    if (decInst.argc > 0) {
        sz += DisassembleOperandToBuf(decInst.operands[decInst.argc-1],
                                 &strbuf[sz], len-sz);
        for(k = decInst.argc-2; k >= 0; k--) {
            sz += snprintf(&strbuf[sz], len-sz, ", ");
            sz += DisassembleOperandToBuf(decInst.operands[k], &strbuf[sz], len-sz);
        }
    }
}

// Disassmble the x86 instruction pointed to by code pointer "stream."
// Put the disassemble text in the "strbuf" up to string length "len".
// Return the code pointer after the disassemble x86 instruction.
extern "C" ENCODER_DECLARE_EXPORT
char* decoder_disassemble_instr(char* stream, char* strbuf, unsigned int len)
{
    Inst decInst;
    unsigned numBytes = DecoderBase::decode(stream, &decInst);
    DisassembleInstToBuf(decInst, strbuf, len);
    return (stream + numBytes);
}
