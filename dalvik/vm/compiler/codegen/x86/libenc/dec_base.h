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
 * @brief Main decoding (disassembling) routines and structures.
 *
 * @note Quick and rough implementation, subject for a change.
 */

#ifndef __DEC_BASE_H_INCLUDED__
#define __DEC_BASE_H_INCLUDED__


#include "enc_base.h"
#include "enc_prvt.h"

#ifdef ENCODER_ISOLATE
using namespace enc_ia32;
#endif

#define IF_CONDITIONAL  (0x00000000)
#define IF_SYMMETRIC    (0x00000000)
#define IF_BRANCH       (0x00000000)

struct Inst {
    Inst() {
        mn = Mnemonic_Null;
        prefc = 0;
        size = 0;
        flags = 0;
        //offset = 0;
        //direct_addr = NULL;
        argc = 0;
        for(int i = 0; i < 4; ++i)
        {
            pref[i] = InstPrefix_Null;
        }
    }
    /**
     * Mnemonic of the instruction.s
     */
    Mnemonic mn;
    /**
     * Enumerating of indexes in the pref array.
     */
    enum PrefGroups
    {
        Group1 = 0,
        Group2,
        Group3,
        Group4
    };
    /**
     * Number of prefixes (1 byte each).
     */
    unsigned int prefc;
    /**
     * Instruction prefixes. Prefix should be placed here according to its group.
     */
    InstPrefix pref[4];
    /**
     * Size, in bytes, of the instruction.
     */
    unsigned size;
    /**
     * Flags of the instruction.
     * @see MF_
     */
    unsigned flags;
    /**
     * An offset of target address, in case of 'CALL offset',
     * 'JMP/Jcc offset'.
     */
    //int      offset;
    /**
     * Direct address of the target (on Intel64/IA-32 is 'instruction IP' +
     * 'instruction length' + offset).
     */
    //void *   direct_addr;
    /**
     * Number of arguments of the instruction.
     */
    unsigned argc;
    //
    EncoderBase::Operand operands[3];
    //
    const EncoderBase::OpcodeDesc * odesc;
};

inline bool is_jcc(Mnemonic mn)
{
    return Mnemonic_JO <= mn && mn<=Mnemonic_JG;
}

class DecoderBase {
public:
    static unsigned decode(const void * addr, Inst * pinst);
private:
    static bool decodeModRM(const EncoderBase::OpcodeDesc& odesc,
        const unsigned char ** pbuf, Inst * pinst
#ifdef _EM64T_
        , const Rex *rex
#endif
        );
    static bool decode_aux(const EncoderBase::OpcodeDesc& odesc,
        unsigned aux, const unsigned char ** pbuf,
        Inst * pinst
#ifdef _EM64T_
        , const Rex *rex
#endif
        );
    static bool try_mn(Mnemonic mn, const unsigned char ** pbuf, Inst * pinst);
    static unsigned int fill_prefs( const unsigned char * bytes, Inst * pinst);
    static bool is_prefix(const unsigned char * bytes);
};

#endif  // ~ __DEC_BASE_H_INCLUDED__

