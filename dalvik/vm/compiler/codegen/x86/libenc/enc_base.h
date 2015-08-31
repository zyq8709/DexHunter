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
 * @brief Main encoding routines and structures.
 */

#ifndef __ENC_BASE_H_INCLUDED__
#define __ENC_BASE_H_INCLUDED__

#include "enc_defs.h"


#include <stdlib.h>
#include <assert.h>
#include <memory.h>

ENCODER_NAMESPACE_START
struct MnemonicInfo;
struct OpcodeInfo;
struct Rex;

/**
 * @brief Basic facilities for generation of processor's instructions.
 *
 * The class EncoderBase represents the basic facilities for the encoding of
 * processor's instructions on IA32 and EM64T platforms.
 *
 * The class provides general interface to generate the instructions as well
 * as to retrieve some static data about instructions (number of arguments,
 * their roles, etc).
 *
 * Currently, the EncoderBase class is used for both LIL and Jitrino code
 * generators. Each of these code generators has its own wrapper to adapt
 * this general interface for specific needs - see encoder.h for LIL wrappers
 * and Ia32Encoder.h for Jitrino's adapter.
 *
 * Interface is provided through static methods, no instances of EncoderBase
 * to be created.
 *
 * @todo RIP-based addressing on EM64T - it's not yet supported currently.
 */
class EncoderBase {
public:
    class Operands;
    struct MnemonicDesc;
    /**
     * @brief Generates processor's instruction.
     *
     * @param stream - a buffer to generate into
     * @param mn - \link Mnemonic mnemonic \endlink of the instruction
     * @param opnds - operands for the instruction
     * @returns (stream + length of the just generated instruction)
     */
    static char * encode(char * stream, Mnemonic mn, const Operands& opnds);
    static char * getOpndLocation(int index);

    /**
     * @brief Generates the smallest possible number of NOP-s.
     *
     * Effectively generates the smallest possible number of instructions,
     * which are NOP-s for CPU. Normally used to make a code alignment.
     *
     * The method inserts exactly number of bytes specified. It's a caller's
     * responsibility to make sure the buffer is big enough.
     *
     * @param stream - buffer where to generate code into, can not be NULL
     * @param howMany - how many bytes to fill with NOP-s
     * @return \c (stream+howMany)
     */
    static char * nops(char * stream, unsigned howMany);

    /**
     * @brief Inserts a prefix into the code buffer.
     *
     * The method writes no more than one byte into the buffer. This is a
     * caller's responsibility to make sure the buffer is big enough.
     *
     * @param stream - buffer where to insert the prefix
     * @param pref - prefix to be inserted. If it's InstPrefix_Null, then
     *        no action performed and return value is \c stream.
     * @return \c (stream+1) if pref is not InstPrefix_Null, or \c stream
     *         otherwise
     */
     static char * prefix(char* stream, InstPrefix pref);

    /**
     * @brief Determines if operand with opndExt suites the position with instExt.
     */
    static bool extAllowed(OpndExt opndExt, OpndExt instExt);

    /**
     * @brief Returns #MnemonicDesc by the given Mnemonic.
     */
    static const MnemonicDesc * getMnemonicDesc(Mnemonic mn)
    {
        assert(mn < Mnemonic_Count);
        return mnemonics + mn;
    }

    /**
     * @brief Returns a Mnemonic for the given name.
     *
     * The lookup is case insensitive, if no mnemonic found for the given
     * string, then Mnemonic_Null returned.
     */
    static Mnemonic str2mnemonic(const char * mn_name);

    /**
     * @brief Returns a string representation of the given Mnemonic.
     *
     * If invalid mnemonic passed, then the behavior is unpredictable.
     */
    static const char * getMnemonicString(Mnemonic mn)
    {
        return getMnemonicDesc(mn)->name;
    }

    static const char * toStr(Mnemonic mn)
    {
        return getMnemonicDesc(mn)->name;
    }


    /**
     * @brief Description of operand.
     *
     * Description of an operand in opcode - its kind, size or RegName if
     * operand must be a particular register.
     */
    struct OpndDesc {
        /**
         * @brief Location of the operand.
         *
         * May be a mask, i.e. OpndKind_Imm|OpndKind_Mem.
         */
        OpndKind        kind;
        /**
         * @brief Size of the operand.
         */
        OpndSize        size;
        /**
         * @brief Extention of the operand.
         */
        OpndExt         ext;
        /**
         * @brief Appropriate RegName if operand must reside on a particular
         *        register (i.e. CWD/CDQ instructions), RegName_Null
         *        otherwise.
         */
        RegName         reg;
    };

    /**
     * @brief Description of operands' roles in instruction.
     */
    struct OpndRolesDesc {
        /**
         * @brief Total number of operands in the operation.
         */
        unsigned                count;
        /**
         * @brief Number of defs in the operation.
         */
        unsigned                defCount;
        /**
         * @brief Number of uses in the operation.
         */
        unsigned                useCount;
        /**
         * @brief Operand roles, bit-packed.
         *
         * A bit-packed info about operands' roles. Each operand's role is
         * described by two bits, counted from right-to-left - the less
         * significant bits (0,1) represent operand#0.
         *
         * The mask is build by ORing #OpndRole_Def and #OpndRole_Use
         * appropriately and shifting left, i.e. operand#0's role would be
         * - '(OpndRole_Def|OpndRole_Use)'
         * - opnd#1's role would be 'OpndRole_Use<<2'
         * - and operand#2's role would be, say, 'OpndRole_Def<<4'.
         */
        unsigned                roles;
    };

    /**
     * @brief Extracts appropriate OpndRole for a given operand.
     *
     * The order of operands is left-to-right, i.e. for MOV, it
     * would be 'MOV op0, op1'
     */
    static OpndRole getOpndRoles(OpndRolesDesc ord, unsigned idx)
    {
        assert(idx < ord.count);
        return (OpndRole)(ord.roles>>((ord.count-1-idx)*2) & 0x3);
    }

    /**
     * @brief Info about single opcode - its opcode bytes, operands,
     *        operands' roles.
     */
   union OpcodeDesc {
       char dummy[128]; // To make total size a power of 2

       struct {
           /**
           * @brief Raw opcode bytes.
           *
           * 'Raw' opcode bytes which do not require any analysis and are
           * independent from arguments/sizes/etc (may include opcode size
           * prefix).
           */
           char        opcode[5];
           unsigned    opcode_len;
           unsigned    aux0;
           unsigned    aux1;
           /**
           * @brief Info about opcode's operands.
           *
           * The [3] mostly comes from IDIV/IMUL which both may have up to 3
           * operands.
           */
           OpndDesc        opnds[3];
           unsigned        first_opnd;
           /**
           * @brief Info about operands - total number, number of uses/defs,
           *        operands' roles.
           */
           OpndRolesDesc   roles;
           /**
           * @brief If not zero, then this is final OpcodeDesc structure in
           *        the list of opcodes for a given mnemonic.
           */
           char            last;
           char            platf;
       };
   };
public:
    /**
     * @brief General info about mnemonic.
     */
    struct MnemonicDesc {
        /**
        * @brief The mnemonic itself.
        */
        Mnemonic        mn;
        /**
        * Various characteristics of mnemonic.
        * @see MF_
         */
        unsigned    flags;
        /**
         * @brief Operation's operand's count and roles.
         *
         * For the operations whose opcodes may use different number of
         * operands (i.e. IMUL/SHL) either most common value used, or empty
         * value left.
         */
        OpndRolesDesc   roles;
        /**
         * @brief Print name of the mnemonic.
         */
        const char *    name;
    };


    /**
     * @brief Magic number, shows a maximum value a hash code can take.
     *
     * For meaning and arithmetics see enc_tabl.cpp.
     *
     * The value was increased from '5155' to '8192' to make it aligned
     * for faster access in EncoderBase::lookup().
     */
    static const unsigned int               HASH_MAX = 8192; //5155;
    /**
     * @brief Empty value, used in hash-to-opcode map to show an empty slot.
     */
    static const unsigned char              NOHASH = 0xFF;
    /**
     * @brief The name says it all.
     */
    static const unsigned char              HASH_BITS_PER_OPERAND = 5;

    /**
     * @brief Contains info about a single instructions's operand - its
     *        location, size and a value for immediate or RegName for
     *        register operands.
     */
    class Operand {
    public:
        /**
         * @brief Initializes the instance with empty size and kind.
         */
        Operand() : m_kind(OpndKind_Null), m_size(OpndSize_Null), m_ext(OpndExt_None), m_need_rex(false) {}
        /**
         * @brief Creates register operand from given RegName.
         */
        Operand(RegName reg, OpndExt ext = OpndExt_None) : m_kind(getRegKind(reg)),
                               m_size(getRegSize(reg)),
                               m_ext(ext), m_reg(reg)
        {
            hash_it();
        }
        /**
         * @brief Creates register operand from given RegName and with the
         *        specified size and kind.
         *
         * Used to speedup Operand creation as there is no need to extract
         * size and kind from the RegName.
         * The provided size and kind must match the RegName's ones though.
         */
        Operand(OpndSize sz, OpndKind kind, RegName reg, OpndExt ext = OpndExt_None) :
            m_kind(kind), m_size(sz), m_ext(ext), m_reg(reg)
        {
            assert(m_size == getRegSize(reg));
            assert(m_kind == getRegKind(reg));
            hash_it();
        }
        /**
         * @brief Creates immediate operand with the given size and value.
         */
        Operand(OpndSize size, long long ival, OpndExt ext = OpndExt_None) :
            m_kind(OpndKind_Imm), m_size(size), m_ext(ext), m_imm64(ival)
        {
            hash_it();
        }
        /**
         * @brief Creates immediate operand of OpndSize_32.
         */
        Operand(int ival, OpndExt ext = OpndExt_None) :
            m_kind(OpndKind_Imm), m_size(OpndSize_32), m_ext(ext), m_imm64(ival)
        {
            hash_it();
        }
        /**
         * @brief Creates immediate operand of OpndSize_16.
         */
        Operand(short ival, OpndExt ext = OpndExt_None) :
            m_kind(OpndKind_Imm), m_size(OpndSize_16), m_ext(ext), m_imm64(ival)
        {
            hash_it();
        }

        /**
         * @brief Creates immediate operand of OpndSize_8.
         */
        Operand(char ival, OpndExt ext = OpndExt_None) :
            m_kind(OpndKind_Imm), m_size(OpndSize_8), m_ext(ext), m_imm64(ival)
        {
            hash_it();
        }

        /**
         * @brief Creates memory operand.
         */
        Operand(OpndSize size, RegName base, RegName index, unsigned scale,
                int disp, OpndExt ext = OpndExt_None) : m_kind(OpndKind_Mem), m_size(size), m_ext(ext)
        {
            m_base = base;
            m_index = index;
            m_scale = scale;
            m_disp = disp;
            hash_it();
        }

        /**
         * @brief Creates memory operand with only base and displacement.
         */
        Operand(OpndSize size, RegName base, int disp, OpndExt ext = OpndExt_None) :
            m_kind(OpndKind_Mem), m_size(size), m_ext(ext)
        {
            m_base = base;
            m_index = RegName_Null;
            m_scale = 0;
            m_disp = disp;
            hash_it();
        }
        //
        // general info
        //
        /**
         * @brief Returns kind of the operand.
         */
        OpndKind kind(void) const { return m_kind; }
        /**
         * @brief Returns size of the operand.
         */
        OpndSize size(void) const { return m_size; }
        /**
         * @brief Returns extention of the operand.
         */
        OpndExt ext(void) const { return m_ext; }
        /**
         * @brief Returns hash of the operand.
         */
        unsigned hash(void) const { return m_hash; }
        //
#ifdef _EM64T_
        bool need_rex(void) const { return m_need_rex; }
#else
        bool need_rex(void) const { return false; }
#endif
        /**
         * @brief Tests whether operand is memory operand.
         */
        bool is_mem(void) const { return is_placed_in(OpndKind_Mem); }
        /**
         * @brief Tests whether operand is immediate operand.
         */
        bool is_imm(void) const { return is_placed_in(OpndKind_Imm); }
        /**
         * @brief Tests whether operand is register operand.
         */
        bool is_reg(void) const { return is_placed_in(OpndKind_Reg); }
        /**
         * @brief Tests whether operand is general-purpose register operand.
         */
        bool is_gpreg(void) const { return is_placed_in(OpndKind_GPReg); }
        /**
         * @brief Tests whether operand is float-point pseudo-register operand.
         */
        bool is_fpreg(void) const { return is_placed_in(OpndKind_FPReg); }
        /**
         * @brief Tests whether operand is XMM register operand.
         */
        bool is_xmmreg(void) const { return is_placed_in(OpndKind_XMMReg); }
#ifdef _HAVE_MMX_
        /**
         * @brief Tests whether operand is MMX register operand.
         */
        bool is_mmxreg(void) const { return is_placed_in(OpndKind_MMXReg); }
#endif
        /**
         * @brief Tests whether operand is signed immediate operand.
         */
        //bool is_signed(void) const { assert(is_imm()); return m_is_signed; }

        /**
         * @brief Returns base of memory operand (RegName_Null if not memory).
         */
        RegName base(void) const { return is_mem() ? m_base : RegName_Null; }
        /**
         * @brief Returns index of memory operand (RegName_Null if not memory).
         */
        RegName index(void) const { return is_mem() ? m_index : RegName_Null; }
        /**
         * @brief Returns scale of memory operand (0 if not memory).
         */
        unsigned scale(void) const { return is_mem() ? m_scale : 0; }
        /**
         * @brief Returns displacement of memory operand (0 if not memory).
         */
        int disp(void) const { return is_mem() ? m_disp : 0; }
        /**
         * @brief Returns RegName of register operand (RegName_Null if not
         *        register).
         */
        RegName reg(void) const { return is_reg() ? m_reg : RegName_Null; }
        /**
         * @brief Returns value of immediate operand (0 if not immediate).
         */
        long long imm(void) const { return is_imm() ? m_imm64 : 0; }
    private:
        bool is_placed_in(OpndKind kd) const
        {
                return kd == OpndKind_Reg ?
                        m_kind == OpndKind_GPReg ||
#ifdef _HAVE_MMX_
                        m_kind == OpndKind_MMXReg ||
#endif
                        m_kind == OpndKind_FPReg ||
                        m_kind == OpndKind_XMMReg
                        : kd == m_kind;
        }
        void hash_it(void)
        {
            m_hash = get_size_hash(m_size) | get_kind_hash(m_kind);
#ifdef _EM64T_
            m_need_rex = false;
            if (is_reg() && is_em64t_extra_reg(m_reg)) {
                m_need_rex = true;
            }
            else if (is_mem() && (is_em64t_extra_reg(m_base) ||
                                  is_em64t_extra_reg(m_index))) {
                m_need_rex = true;
            }
#endif
        }
        // general info
        OpndKind    m_kind;
        OpndSize    m_size;
        OpndExt     m_ext;
        // complex address form support
        RegName     m_base;
        RegName     m_index;
        unsigned    m_scale;
        union {
            int         m_disp;
            RegName     m_reg;
            long long   m_imm64;
        };
        unsigned    m_hash;
        bool        m_need_rex;
        friend class EncoderBase::Operands;
    };
    /**
     * @brief Simple container for up to 3 Operand-s.
     */
    class Operands {
    public:
        Operands(void)
        {
            clear();
        }
        Operands(const Operand& op0)
        {
            clear();
            add(op0);
        }

        Operands(const Operand& op0, const Operand& op1)
        {
            clear();
            add(op0); add(op1);
        }

        Operands(const Operand& op0, const Operand& op1, const Operand& op2)
        {
            clear();
            add(op0); add(op1); add(op2);
        }

        unsigned count(void) const { return m_count; }
        unsigned hash(void) const { return m_hash; }
        const Operand& operator[](unsigned idx) const
        {
            assert(idx<m_count);
            return m_operands[idx];
        }

        void add(const Operand& op)
        {
            assert(m_count < COUNTOF(m_operands));
            m_hash = (m_hash<<HASH_BITS_PER_OPERAND) | op.hash();
            m_operands[m_count++] = op;
            m_need_rex = m_need_rex || op.m_need_rex;
        }
#ifdef _EM64T_
        bool need_rex(void) const { return m_need_rex; }
#else
        bool need_rex(void) const { return false; }
#endif
        void clear(void)
        {
            m_count = 0; m_hash = 0; m_need_rex = false;
        }
    private:
        unsigned    m_count;
        Operand     m_operands[COUNTOF( ((OpcodeDesc*)NULL)->opnds )];
        unsigned    m_hash;
        bool        m_need_rex;
    };
public:
#ifdef _DEBUG
    /**
     * Verifies some presumptions about encoding data table.
     * Called automaticaly during statics initialization.
     */
    static int verify(void);
#endif

private:
    /**
     * @brief Returns found OpcodeDesc by the given Mnemonic and operands.
     */
    static const OpcodeDesc * lookup(Mnemonic mn, const Operands& opnds);
    /**
     * @brief Encodes mod/rm byte.
     */
    static char* encodeModRM(char* stream, const Operands& opnds,
                             unsigned idx, const OpcodeDesc * odesc, Rex * prex);
    /**
     * @brief Encodes special things of opcode description - '/r', 'ib', etc.
     */
    static char* encode_aux(char* stream, unsigned aux,
                            const Operands& opnds, const OpcodeDesc * odesc,
                            unsigned * pargsCount, Rex* prex);
#ifdef _EM64T_
    /**
     * @brief Returns true if the 'reg' argument represents one of the new
     *        EM64T registers - R8(D)-R15(D).
     *
     * The 64 bits versions of 'old-fashion' registers, i.e. RAX are not
     * considered as 'extra'.
     */
    static bool is_em64t_extra_reg(const RegName reg)
    {
        if (needs_rex_r(reg)) {
            return true;
        }
        if (RegName_SPL <= reg && reg <= RegName_R15L) {
            return true;
        }
        return false;
    }
    static bool needs_rex_r(const RegName reg)
    {
        if (RegName_R8 <= reg && reg <= RegName_R15) {
            return true;
        }
        if (RegName_R8D <= reg && reg <= RegName_R15D) {
            return true;
        }
        if (RegName_R8S <= reg && reg <= RegName_R15S) {
            return true;
        }
        if (RegName_R8L <= reg && reg <= RegName_R15L) {
            return true;
        }
        if (RegName_XMM8 <= reg && reg <= RegName_XMM15) {
            return true;
        }
        if (RegName_XMM8D <= reg && reg <= RegName_XMM15D) {
            return true;
        }
        if (RegName_XMM8S <= reg && reg <= RegName_XMM15S) {
            return true;
        }
        return false;
    }
    /**
     * @brief Returns an 'processor's index' of the register - the index
     *        used to encode the register in ModRM/SIB bytes.
     *
     * For the new EM64T registers the 'HW index' differs from the index
     * encoded in RegName. For old-fashion registers it's effectively the
     * same as ::getRegIndex(RegName).
     */
    static unsigned char getHWRegIndex(const RegName reg)
    {
        if (getRegKind(reg) != OpndKind_GPReg) {
            return getRegIndex(reg);
        }
        if (RegName_SPL <= reg && reg<=RegName_DIL) {
            return getRegIndex(reg);
        }
        if (RegName_R8L<= reg && reg<=RegName_R15L) {
            return getRegIndex(reg) - getRegIndex(RegName_R8L);
        }
        return is_em64t_extra_reg(reg) ?
                getRegIndex(reg)-getRegIndex(RegName_R8D) : getRegIndex(reg);
    }
#else
    static unsigned char getHWRegIndex(const RegName reg)
    {
        return getRegIndex(reg);
    }
    static bool is_em64t_extra_reg(const RegName reg)
    {
        return false;
    }
#endif
public:
    static unsigned char get_size_hash(OpndSize size) {
        return (size <= OpndSize_64) ? size_hash[size] : 0xFF;
    }
    static unsigned char get_kind_hash(OpndKind kind) {
        return (kind <= OpndKind_Mem) ? kind_hash[kind] : 0xFF;
    }

    /**
     * @brief A table used for the fast computation of hash value.
     *
     * A change must be strictly balanced with hash-related functions and data
     * in enc_base.h/.cpp.
     */
    static const unsigned char size_hash[OpndSize_64+1];
    /**
     * @brief A table used for the fast computation of hash value.
     *
     * A change must be strictly balanced with hash-related functions and data
     * in enc_base.h/.cpp.
     */
    static const unsigned char kind_hash[OpndKind_Mem+1];
    /**
     * @brief Maximum number of opcodes used for a single mnemonic.
     *
     * No arithmetics behind the number, simply estimated.
     */
    static const unsigned int   MAX_OPCODES = 32; //20;
    /**
     * @brief Mapping between operands hash code and operands.
     */
    static unsigned char    opcodesHashMap[Mnemonic_Count][HASH_MAX];
    /**
     * @brief Array of mnemonics.
     */
    static MnemonicDesc         mnemonics[Mnemonic_Count];
    /**
     * @brief Array of available opcodes.
     */
    static OpcodeDesc opcodes[Mnemonic_Count][MAX_OPCODES];

    static int buildTable(void);
    static void buildMnemonicDesc(const MnemonicInfo * minfo);
    /**
     * @brief Computes hash value for the given operands.
     */
    static unsigned short getHash(const OpcodeInfo* odesc);
    /**
     * @brief Dummy variable, for automatic invocation of buildTable() at
     *        startup.
     */
    static int dummy;

    static char * curRelOpnd[3];
};

ENCODER_NAMESPACE_END

#endif // ifndef __ENC_BASE_H_INCLUDED__
