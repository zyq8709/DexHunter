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

/**
 * @file
 * @brief Main decoding (disassembling) routines implementation.
 */

#include "dec_base.h"
#include "enc_prvt.h"
#include <stdio.h>
//#include "open/common.h"

bool DecoderBase::is_prefix(const unsigned char * bytes)
{
    unsigned char b0 = *bytes;
    unsigned char b1 = *(bytes+1);
    if (b0 == 0xF0) { // LOCK
        return true;
    }
    if (b0==0xF2 || b0==0xF3) { // REPNZ/REPZ prefixes
        if (b1 == 0x0F) {   // .... but may be a part of SIMD opcode
            return false;
        }
        return true;
    }
    if (b0 == 0x2E || b0 == 0x36 || b0==0x3E || b0==0x26 || b0==0x64 || b0==0x3E) {
        // branch hints, segment prefixes
        return true;
    }
    if (b0==0x66) { // operand-size prefix
        if (b1 == 0x0F) {   // .... but may be a part of SIMD opcode
            return false;
        }
        return false; //XXX - currently considered as part of opcode//true;
    }
    if (b0==0x67) { // address size prefix
        return true;
    }
    return false;
}

// Returns prefix count from 0 to 4, or ((unsigned int)-1) on error
unsigned int DecoderBase::fill_prefs(const unsigned char * bytes, Inst * pinst)
{
    const unsigned char * my_bytes = bytes;

    while( 1 )
    {
        unsigned char by1 = *my_bytes;
        unsigned char by2 = *(my_bytes + 1);
        Inst::PrefGroups where;

        switch( by1 )
        {
        case InstPrefix_REPNE:
        case InstPrefix_REP:
        {
            if( 0x0F == by2)
            {
                return pinst->prefc;
            }
        }
        case InstPrefix_LOCK:
        {
            where = Inst::Group1;
            break;
        }
        case InstPrefix_CS:
        case InstPrefix_SS:
        case InstPrefix_DS:
        case InstPrefix_ES:
        case InstPrefix_FS:
        case InstPrefix_GS:
//      case InstPrefix_HintTaken: the same as CS override
//      case InstPrefix_HintNotTaken: the same as DS override
        {
            where = Inst::Group2;
            break;
        }
        case InstPrefix_OpndSize:
        {
//NOTE:   prefix does not work for JMP Sz16, the opcode is 0x66 0xe9
//        here 0x66 will be treated as prefix, try_mn will try to match the code starting at 0xe9
//        it will match JMP Sz32 ...
//HACK:   assume it is the last prefix, return any way
            if( 0x0F == by2)
            {
                return pinst->prefc;
            }
            return pinst->prefc;
            where = Inst::Group3;
            break;
        }
        case InstPrefix_AddrSize:
        {
            where = Inst::Group4;
            break;
        }
        default:
        {
            return pinst->prefc;
        }
        }
        // Assertions are not allowed here.
        // Error situations should result in returning error status
        if (InstPrefix_Null != pinst->pref[where]) //only one prefix in each group
            return (unsigned int)-1;

        pinst->pref[where] = (InstPrefix)by1;

        if (pinst->prefc >= 4) //no more than 4 prefixes
            return (unsigned int)-1;

        pinst->prefc++;
        ++my_bytes;
    }
}



unsigned DecoderBase::decode(const void * addr, Inst * pinst)
{
    Inst tmp;

    //assert( *(unsigned char*)addr != 0x66);

    const unsigned char * bytes = (unsigned char*)addr;

    // Load up to 4 prefixes
    // for each Mnemonic
    unsigned int pref_count = fill_prefs(bytes, &tmp);

    if (pref_count == (unsigned int)-1) // Wrong prefix sequence, or >4 prefixes
        return 0; // Error

    bytes += pref_count;

    //  for each opcodedesc
    //      if (raw_len == 0) memcmp(, raw_len)
    //  else check the mixed state which is one of the following:
    //      /digit /i /rw /rd /rb

    bool found = false;
    const unsigned char * saveBytes = bytes;
    for (unsigned mn=1; mn<Mnemonic_Count; mn++) {
        bytes = saveBytes;
        found=try_mn((Mnemonic)mn, &bytes, &tmp);
        if (found) {
            tmp.mn = (Mnemonic)mn;
            break;
        }
    }
    if (!found) {
        // Unknown opcode
        return 0;
    }
    tmp.size = (unsigned)(bytes-(const unsigned char*)addr);
    if (pinst) {
        *pinst = tmp;
    }
    return tmp.size;
}

#ifdef _EM64T_
#define EXTEND_REG(reg, flag)                        \
    ((NULL == rex || 0 == rex->flag) ? reg : (reg + 8))
#else
#define EXTEND_REG(reg, flag) (reg)
#endif

//don't know the use of rex, seems not used when _EM64T_ is not enabled
bool DecoderBase::decode_aux(const EncoderBase::OpcodeDesc& odesc, unsigned aux,
    const unsigned char ** pbuf, Inst * pinst
#ifdef _EM64T_
    , const Rex UNREF *rex
#endif
    )
{
    OpcodeByteKind kind = (OpcodeByteKind)(aux & OpcodeByteKind_KindMask);
    unsigned byte = (aux & OpcodeByteKind_OpcodeMask);
    unsigned data_byte = **pbuf;
    EncoderBase::Operand& opnd = pinst->operands[pinst->argc];
    const EncoderBase::OpndDesc& opndDesc = odesc.opnds[pinst->argc];

    switch (kind) {
    case OpcodeByteKind_SlashR:
        {
            RegName reg;
            OpndKind okind;
            const ModRM& modrm = *(ModRM*)*pbuf;
            if (opndDesc.kind & OpndKind_Mem) { // 1st operand is memory
#ifdef _EM64T_
                decodeModRM(odesc, pbuf, pinst, rex);
#else
                decodeModRM(odesc, pbuf, pinst);
#endif
                ++pinst->argc;
                const EncoderBase::OpndDesc& opndDesc2 = odesc.opnds[pinst->argc];
                okind = ((opndDesc2.kind & OpndKind_XMMReg) || opndDesc2.size==OpndSize_64) ? OpndKind_XMMReg : OpndKind_GPReg;
                EncoderBase::Operand& regOpnd = pinst->operands[pinst->argc];
                reg = getRegName(okind, opndDesc2.size, EXTEND_REG(modrm.reg, r));
                regOpnd = EncoderBase::Operand(reg);
            } else {                            // 2nd operand is memory
                okind = ((opndDesc.kind & OpndKind_XMMReg) || opndDesc.size==OpndSize_64) ? OpndKind_XMMReg : OpndKind_GPReg;
                EncoderBase::Operand& regOpnd = pinst->operands[pinst->argc];
                reg = getRegName(okind, opndDesc.size, EXTEND_REG(modrm.reg, r));
                regOpnd = EncoderBase::Operand(reg);
                ++pinst->argc;
#ifdef _EM64T_
                decodeModRM(odesc, pbuf, pinst, rex);
#else
                decodeModRM(odesc, pbuf, pinst);
#endif
            }
            ++pinst->argc;
        }
        return true;
    case OpcodeByteKind_rb:
    case OpcodeByteKind_rw:
    case OpcodeByteKind_rd:
        {
            // Gregory -
            // Here we don't parse register because for current needs
            // disassembler doesn't require to parse all operands
            unsigned regid = data_byte - byte;
            if (regid>7) {
                return false;
            }
            OpndSize opnd_size;
            switch(kind)
            {
            case OpcodeByteKind_rb:
            {
                opnd_size = OpndSize_8;
                break;
            }
            case OpcodeByteKind_rw:
            {
                opnd_size = OpndSize_16;
                break;
            }
            case OpcodeByteKind_rd:
            {
                opnd_size = OpndSize_32;
                break;
            }
            default:
                opnd_size = OpndSize_32;  // so there is no compiler warning
                assert( false );
            }
            opnd = EncoderBase::Operand( getRegName(OpndKind_GPReg, opnd_size, regid) );

            ++pinst->argc;
            ++*pbuf;
            return true;
        }
    case OpcodeByteKind_cb:
        {
        char offset = *(char*)*pbuf;
        *pbuf += 1;
        opnd = EncoderBase::Operand(offset);
        ++pinst->argc;
        //pinst->direct_addr = (void*)(pinst->offset + *pbuf);
        }
        return true;
    case OpcodeByteKind_cw:
        // not an error, but not expected in current env
        // Android x86
        {
        short offset = *(short*)*pbuf;
        *pbuf += 2;
        opnd = EncoderBase::Operand(offset);
        ++pinst->argc;
        }
        return true;
        //return false;
    case OpcodeByteKind_cd:
        {
        int offset = *(int*)*pbuf;
        *pbuf += 4;
        opnd = EncoderBase::Operand(offset);
        ++pinst->argc;
        }
        return true;
    case OpcodeByteKind_SlashNum:
        {
        const ModRM& modrm = *(ModRM*)*pbuf;
        if (modrm.reg != byte) {
            return false;
        }
        decodeModRM(odesc, pbuf, pinst
#ifdef _EM64T_
                        , rex
#endif
                        );
        ++pinst->argc;
        }
        return true;
    case OpcodeByteKind_ib:
        {
        char ival = *(char*)*pbuf;
        opnd = EncoderBase::Operand(ival);
        ++pinst->argc;
        *pbuf += 1;
        }
        return true;
    case OpcodeByteKind_iw:
        {
        short ival = *(short*)*pbuf;
        opnd = EncoderBase::Operand(ival);
        ++pinst->argc;
        *pbuf += 2;
        }
        return true;
    case OpcodeByteKind_id:
        {
        int ival = *(int*)*pbuf;
        opnd = EncoderBase::Operand(ival);
        ++pinst->argc;
        *pbuf += 4;
        }
        return true;
#ifdef _EM64T_
    case OpcodeByteKind_io:
        {
        long long int ival = *(long long int*)*pbuf;
        opnd = EncoderBase::Operand(OpndSize_64, ival);
        ++pinst->argc;
        *pbuf += 8;
        }
        return true;
#endif
    case OpcodeByteKind_plus_i:
        {
            unsigned regid = data_byte - byte;
            if (regid>7) {
                return false;
            }
            ++*pbuf;
            return true;
        }
    case OpcodeByteKind_ZeroOpcodeByte: // cant be here
        return false;
    default:
        // unknown kind ? how comes ?
        break;
    }
    return false;
}

bool DecoderBase::try_mn(Mnemonic mn, const unsigned char ** pbuf, Inst * pinst) {
    const unsigned char * save_pbuf = *pbuf;
    EncoderBase::OpcodeDesc * opcodes = EncoderBase::opcodes[mn];

    for (unsigned i=0; !opcodes[i].last; i++) {
        const EncoderBase::OpcodeDesc& odesc = opcodes[i];
        char *opcode_ptr = const_cast<char *>(odesc.opcode);
        int opcode_len = odesc.opcode_len;
#ifdef _EM64T_
        Rex *prex = NULL;
        Rex rex;
#endif

        *pbuf = save_pbuf;
#ifdef _EM64T_
        // Match REX prefixes
        unsigned char rex_byte = (*pbuf)[0];
        if ((rex_byte & 0xf0) == 0x40)
        {
            if ((rex_byte & 0x08) != 0)
            {
                // Have REX.W
                if (opcode_len > 0 && opcode_ptr[0] == 0x48)
                {
                    // Have REX.W in opcode. All mnemonics that allow
                    // REX.W have to have specified it in opcode,
                    // otherwise it is not allowed
                    rex = *(Rex *)*pbuf;
                    prex = &rex;
                    (*pbuf)++;
                    opcode_ptr++;
                    opcode_len--;
                }
            }
            else
            {
                // No REX.W, so it doesn't have to be in opcode. We
                // have REX.B, REX.X, REX.R or their combination, but
                // not in opcode, they may extend any part of the
                // instruction
                rex = *(Rex *)*pbuf;
                prex = &rex;
                (*pbuf)++;
            }
        }
#endif
        if (opcode_len != 0) {
            if (memcmp(*pbuf, opcode_ptr, opcode_len)) {
                continue;
            }
            *pbuf += opcode_len;
        }
        if (odesc.aux0 != 0) {

            if (!decode_aux(odesc, odesc.aux0, pbuf, pinst
#ifdef _EM64T_
                            , prex
#endif
                            )) {
                continue;
            }
            if (odesc.aux1 != 0) {
                if (!decode_aux(odesc, odesc.aux1, pbuf, pinst
#ifdef _EM64T_
                            , prex
#endif
                            )) {
                    continue;
                }
            }
            pinst->odesc = &opcodes[i];
            return true;
        }
        else {
            // Can't have empty opcode
            assert(opcode_len != 0);
            pinst->odesc = &opcodes[i];
            return true;
        }
    }
    return false;
}

bool DecoderBase::decodeModRM(const EncoderBase::OpcodeDesc& odesc,
    const unsigned char ** pbuf, Inst * pinst
#ifdef _EM64T_
    , const Rex *rex
#endif
    )
{
    EncoderBase::Operand& opnd = pinst->operands[pinst->argc];
    const EncoderBase::OpndDesc& opndDesc = odesc.opnds[pinst->argc];

    //XXX debug ///assert(0x66 != *(*pbuf-2));
    const ModRM& modrm = *(ModRM*)*pbuf;
    *pbuf += 1;

    RegName base = RegName_Null;
    RegName index = RegName_Null;
    int disp = 0;
    unsigned scale = 0;

    // On x86_64 all mnemonics that allow REX.W have REX.W in opcode.
    // Therefore REX.W is simply ignored, and opndDesc.size is used

    if (modrm.mod == 3) {
        // we have only modrm. no sib, no disp.
        // Android x86: Use XMMReg for 64b operand.
        OpndKind okind = ((opndDesc.kind & OpndKind_XMMReg) || opndDesc.size == OpndSize_64) ? OpndKind_XMMReg : OpndKind_GPReg;
        RegName reg = getRegName(okind, opndDesc.size, EXTEND_REG(modrm.rm, b));
        opnd = EncoderBase::Operand(reg);
        return true;
    }
    //Android x86: m16, m32, m64: mean a byte[word|doubleword] operand in memory
    //base and index should be 32 bits!!!
    const SIB& sib = *(SIB*)*pbuf;
    // check whether we have a sib
    if (modrm.rm == 4) {
        // yes, we have SIB
        *pbuf += 1;
        // scale = sib.scale == 0 ? 0 : (1<<sib.scale);
        scale = (1<<sib.scale);
        if (sib.index != 4) {
            index = getRegName(OpndKind_GPReg, OpndSize_32, EXTEND_REG(sib.index, x)); //Android x86: OpndDesc.size
        } else {
            // (sib.index == 4) => no index
            //%esp can't be sib.index
        }

        if (sib.base != 5 || modrm.mod != 0) {
            base = getRegName(OpndKind_GPReg, OpndSize_32, EXTEND_REG(sib.base, b)); //Android x86: OpndDesc.size
        } else {
            // (sib.base == 5 && modrm.mod == 0) => no base
        }
    }
    else {
        if (modrm.mod != 0 || modrm.rm != 5) {
            base = getRegName(OpndKind_GPReg, OpndSize_32, EXTEND_REG(modrm.rm, b)); //Android x86: OpndDesc.size
        }
        else {
            // mod=0 && rm == 5 => only disp32
        }
    }

    //update disp and pbuf
    if (modrm.mod == 2) {
        // have disp32
        disp = *(int*)*pbuf;
        *pbuf += 4;
    }
    else if (modrm.mod == 1) {
        // have disp8
        disp = *(char*)*pbuf;
        *pbuf += 1;
    }
    else {
        assert(modrm.mod == 0);
        if (modrm.rm == 5) {
            // have disp32 w/o sib
            disp = *(int*)*pbuf;
            *pbuf += 4;
        }
        else if (modrm.rm == 4 && sib.base == 5) {
            // have disp32 with SI in sib
            disp = *(int*)*pbuf;
            *pbuf += 4;
        }
    }
    opnd = EncoderBase::Operand(opndDesc.size, base, index, scale, disp);
    return true;
}

