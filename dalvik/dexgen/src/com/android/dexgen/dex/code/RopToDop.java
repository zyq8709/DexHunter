/*
 * Copyright (C) 2007 The Android Open Source Project
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

package com.android.dexgen.dex.code;

import com.android.dexgen.rop.code.Insn;
import com.android.dexgen.rop.code.RegOps;
import com.android.dexgen.rop.code.RegisterSpec;
import com.android.dexgen.rop.code.Rop;
import com.android.dexgen.rop.code.Rops;
import com.android.dexgen.rop.code.ThrowingCstInsn;
import com.android.dexgen.rop.cst.Constant;
import com.android.dexgen.rop.cst.CstFieldRef;
import com.android.dexgen.rop.cst.CstString;
import com.android.dexgen.rop.cst.CstType;
import com.android.dexgen.rop.type.Type;

import java.util.HashMap;

/**
 * Translator from rop-level {@link Insn} instances to corresponding
 * {@link Dop} instances.
 */
public final class RopToDop {
    /** {@code non-null;} map from all the common rops to dalvik opcodes */
    private static final HashMap<Rop, Dop> MAP;

    /**
     * This class is uninstantiable.
     */
    private RopToDop() {
        // This space intentionally left blank.
    }

    static {
        /*
         * Note: The choices made here are to pick the optimistically
         * smallest Dalvik opcode, and leave it to later processing to
         * pessimize.
         */
        MAP = new HashMap<Rop, Dop>(400);
        MAP.put(Rops.NOP,               Dops.NOP);
        MAP.put(Rops.MOVE_INT,          Dops.MOVE);
        MAP.put(Rops.MOVE_LONG,         Dops.MOVE_WIDE);
        MAP.put(Rops.MOVE_FLOAT,        Dops.MOVE);
        MAP.put(Rops.MOVE_DOUBLE,       Dops.MOVE_WIDE);
        MAP.put(Rops.MOVE_OBJECT,       Dops.MOVE_OBJECT);
        MAP.put(Rops.MOVE_PARAM_INT,    Dops.MOVE);
        MAP.put(Rops.MOVE_PARAM_LONG,   Dops.MOVE_WIDE);
        MAP.put(Rops.MOVE_PARAM_FLOAT,  Dops.MOVE);
        MAP.put(Rops.MOVE_PARAM_DOUBLE, Dops.MOVE_WIDE);
        MAP.put(Rops.MOVE_PARAM_OBJECT, Dops.MOVE_OBJECT);

        /*
         * Note: No entry for MOVE_EXCEPTION, since it varies by
         * exception type. (That is, there is no unique instance to
         * add to the map.)
         */

        MAP.put(Rops.CONST_INT,         Dops.CONST_4);
        MAP.put(Rops.CONST_LONG,        Dops.CONST_WIDE_16);
        MAP.put(Rops.CONST_FLOAT,       Dops.CONST_4);
        MAP.put(Rops.CONST_DOUBLE,      Dops.CONST_WIDE_16);

        /*
         * Note: No entry for CONST_OBJECT, since it needs to turn
         * into either CONST_STRING or CONST_CLASS.
         */

        /*
         * TODO: I think the only case of this is for null, and
         * const/4 should cover that.
         */
        MAP.put(Rops.CONST_OBJECT_NOTHROW, Dops.CONST_4);

        MAP.put(Rops.GOTO,                 Dops.GOTO);
        MAP.put(Rops.IF_EQZ_INT,           Dops.IF_EQZ);
        MAP.put(Rops.IF_NEZ_INT,           Dops.IF_NEZ);
        MAP.put(Rops.IF_LTZ_INT,           Dops.IF_LTZ);
        MAP.put(Rops.IF_GEZ_INT,           Dops.IF_GEZ);
        MAP.put(Rops.IF_LEZ_INT,           Dops.IF_LEZ);
        MAP.put(Rops.IF_GTZ_INT,           Dops.IF_GTZ);
        MAP.put(Rops.IF_EQZ_OBJECT,        Dops.IF_EQZ);
        MAP.put(Rops.IF_NEZ_OBJECT,        Dops.IF_NEZ);
        MAP.put(Rops.IF_EQ_INT,            Dops.IF_EQ);
        MAP.put(Rops.IF_NE_INT,            Dops.IF_NE);
        MAP.put(Rops.IF_LT_INT,            Dops.IF_LT);
        MAP.put(Rops.IF_GE_INT,            Dops.IF_GE);
        MAP.put(Rops.IF_LE_INT,            Dops.IF_LE);
        MAP.put(Rops.IF_GT_INT,            Dops.IF_GT);
        MAP.put(Rops.IF_EQ_OBJECT,         Dops.IF_EQ);
        MAP.put(Rops.IF_NE_OBJECT,         Dops.IF_NE);
        MAP.put(Rops.SWITCH,               Dops.SPARSE_SWITCH);
        MAP.put(Rops.ADD_INT,              Dops.ADD_INT_2ADDR);
        MAP.put(Rops.ADD_LONG,             Dops.ADD_LONG_2ADDR);
        MAP.put(Rops.ADD_FLOAT,            Dops.ADD_FLOAT_2ADDR);
        MAP.put(Rops.ADD_DOUBLE,           Dops.ADD_DOUBLE_2ADDR);
        MAP.put(Rops.SUB_INT,              Dops.SUB_INT_2ADDR);
        MAP.put(Rops.SUB_LONG,             Dops.SUB_LONG_2ADDR);
        MAP.put(Rops.SUB_FLOAT,            Dops.SUB_FLOAT_2ADDR);
        MAP.put(Rops.SUB_DOUBLE,           Dops.SUB_DOUBLE_2ADDR);
        MAP.put(Rops.MUL_INT,              Dops.MUL_INT_2ADDR);
        MAP.put(Rops.MUL_LONG,             Dops.MUL_LONG_2ADDR);
        MAP.put(Rops.MUL_FLOAT,            Dops.MUL_FLOAT_2ADDR);
        MAP.put(Rops.MUL_DOUBLE,           Dops.MUL_DOUBLE_2ADDR);
        MAP.put(Rops.DIV_INT,              Dops.DIV_INT_2ADDR);
        MAP.put(Rops.DIV_LONG,             Dops.DIV_LONG_2ADDR);
        MAP.put(Rops.DIV_FLOAT,            Dops.DIV_FLOAT_2ADDR);
        MAP.put(Rops.DIV_DOUBLE,           Dops.DIV_DOUBLE_2ADDR);
        MAP.put(Rops.REM_INT,              Dops.REM_INT_2ADDR);
        MAP.put(Rops.REM_LONG,             Dops.REM_LONG_2ADDR);
        MAP.put(Rops.REM_FLOAT,            Dops.REM_FLOAT_2ADDR);
        MAP.put(Rops.REM_DOUBLE,           Dops.REM_DOUBLE_2ADDR);
        MAP.put(Rops.NEG_INT,              Dops.NEG_INT);
        MAP.put(Rops.NEG_LONG,             Dops.NEG_LONG);
        MAP.put(Rops.NEG_FLOAT,            Dops.NEG_FLOAT);
        MAP.put(Rops.NEG_DOUBLE,           Dops.NEG_DOUBLE);
        MAP.put(Rops.AND_INT,              Dops.AND_INT_2ADDR);
        MAP.put(Rops.AND_LONG,             Dops.AND_LONG_2ADDR);
        MAP.put(Rops.OR_INT,               Dops.OR_INT_2ADDR);
        MAP.put(Rops.OR_LONG,              Dops.OR_LONG_2ADDR);
        MAP.put(Rops.XOR_INT,              Dops.XOR_INT_2ADDR);
        MAP.put(Rops.XOR_LONG,             Dops.XOR_LONG_2ADDR);
        MAP.put(Rops.SHL_INT,              Dops.SHL_INT_2ADDR);
        MAP.put(Rops.SHL_LONG,             Dops.SHL_LONG_2ADDR);
        MAP.put(Rops.SHR_INT,              Dops.SHR_INT_2ADDR);
        MAP.put(Rops.SHR_LONG,             Dops.SHR_LONG_2ADDR);
        MAP.put(Rops.USHR_INT,             Dops.USHR_INT_2ADDR);
        MAP.put(Rops.USHR_LONG,            Dops.USHR_LONG_2ADDR);
        MAP.put(Rops.NOT_INT,              Dops.NOT_INT);
        MAP.put(Rops.NOT_LONG,             Dops.NOT_LONG);

        MAP.put(Rops.ADD_CONST_INT,        Dops.ADD_INT_LIT8);
        // Note: No dalvik ops for other types of add_const.

        /*
         * Note: No dalvik ops for any type of sub_const; there's a
         * *reverse* sub (constant - reg) for ints, though, but that
         * should end up getting handled at optimization time.
         */

        MAP.put(Rops.MUL_CONST_INT,        Dops.MUL_INT_LIT8);
        // Note: No dalvik ops for other types of mul_const.

        MAP.put(Rops.DIV_CONST_INT,        Dops.DIV_INT_LIT8);
        // Note: No dalvik ops for other types of div_const.

        MAP.put(Rops.REM_CONST_INT,        Dops.REM_INT_LIT8);
        // Note: No dalvik ops for other types of rem_const.

        MAP.put(Rops.AND_CONST_INT,        Dops.AND_INT_LIT8);
        // Note: No dalvik op for and_const_long.

        MAP.put(Rops.OR_CONST_INT,         Dops.OR_INT_LIT8);
        // Note: No dalvik op for or_const_long.

        MAP.put(Rops.XOR_CONST_INT,        Dops.XOR_INT_LIT8);
        // Note: No dalvik op for xor_const_long.

        MAP.put(Rops.SHL_CONST_INT,        Dops.SHL_INT_LIT8);
        // Note: No dalvik op for shl_const_long.

        MAP.put(Rops.SHR_CONST_INT,        Dops.SHR_INT_LIT8);
        // Note: No dalvik op for shr_const_long.

        MAP.put(Rops.USHR_CONST_INT,       Dops.USHR_INT_LIT8);
        // Note: No dalvik op for shr_const_long.

        MAP.put(Rops.CMPL_LONG,            Dops.CMP_LONG);
        MAP.put(Rops.CMPL_FLOAT,           Dops.CMPL_FLOAT);
        MAP.put(Rops.CMPL_DOUBLE,          Dops.CMPL_DOUBLE);
        MAP.put(Rops.CMPG_FLOAT,           Dops.CMPG_FLOAT);
        MAP.put(Rops.CMPG_DOUBLE,          Dops.CMPG_DOUBLE);
        MAP.put(Rops.CONV_L2I,             Dops.LONG_TO_INT);
        MAP.put(Rops.CONV_F2I,             Dops.FLOAT_TO_INT);
        MAP.put(Rops.CONV_D2I,             Dops.DOUBLE_TO_INT);
        MAP.put(Rops.CONV_I2L,             Dops.INT_TO_LONG);
        MAP.put(Rops.CONV_F2L,             Dops.FLOAT_TO_LONG);
        MAP.put(Rops.CONV_D2L,             Dops.DOUBLE_TO_LONG);
        MAP.put(Rops.CONV_I2F,             Dops.INT_TO_FLOAT);
        MAP.put(Rops.CONV_L2F,             Dops.LONG_TO_FLOAT);
        MAP.put(Rops.CONV_D2F,             Dops.DOUBLE_TO_FLOAT);
        MAP.put(Rops.CONV_I2D,             Dops.INT_TO_DOUBLE);
        MAP.put(Rops.CONV_L2D,             Dops.LONG_TO_DOUBLE);
        MAP.put(Rops.CONV_F2D,             Dops.FLOAT_TO_DOUBLE);
        MAP.put(Rops.TO_BYTE,              Dops.INT_TO_BYTE);
        MAP.put(Rops.TO_CHAR,              Dops.INT_TO_CHAR);
        MAP.put(Rops.TO_SHORT,             Dops.INT_TO_SHORT);
        MAP.put(Rops.RETURN_VOID,          Dops.RETURN_VOID);
        MAP.put(Rops.RETURN_INT,           Dops.RETURN);
        MAP.put(Rops.RETURN_LONG,          Dops.RETURN_WIDE);
        MAP.put(Rops.RETURN_FLOAT,         Dops.RETURN);
        MAP.put(Rops.RETURN_DOUBLE,        Dops.RETURN_WIDE);
        MAP.put(Rops.RETURN_OBJECT,        Dops.RETURN_OBJECT);
        MAP.put(Rops.ARRAY_LENGTH,         Dops.ARRAY_LENGTH);
        MAP.put(Rops.THROW,                Dops.THROW);
        MAP.put(Rops.MONITOR_ENTER,        Dops.MONITOR_ENTER);
        MAP.put(Rops.MONITOR_EXIT,         Dops.MONITOR_EXIT);
        MAP.put(Rops.AGET_INT,             Dops.AGET);
        MAP.put(Rops.AGET_LONG,            Dops.AGET_WIDE);
        MAP.put(Rops.AGET_FLOAT,           Dops.AGET);
        MAP.put(Rops.AGET_DOUBLE,          Dops.AGET_WIDE);
        MAP.put(Rops.AGET_OBJECT,          Dops.AGET_OBJECT);
        MAP.put(Rops.AGET_BOOLEAN,         Dops.AGET_BOOLEAN);
        MAP.put(Rops.AGET_BYTE,            Dops.AGET_BYTE);
        MAP.put(Rops.AGET_CHAR,            Dops.AGET_CHAR);
        MAP.put(Rops.AGET_SHORT,           Dops.AGET_SHORT);
        MAP.put(Rops.APUT_INT,             Dops.APUT);
        MAP.put(Rops.APUT_LONG,            Dops.APUT_WIDE);
        MAP.put(Rops.APUT_FLOAT,           Dops.APUT);
        MAP.put(Rops.APUT_DOUBLE,          Dops.APUT_WIDE);
        MAP.put(Rops.APUT_OBJECT,          Dops.APUT_OBJECT);
        MAP.put(Rops.APUT_BOOLEAN,         Dops.APUT_BOOLEAN);
        MAP.put(Rops.APUT_BYTE,            Dops.APUT_BYTE);
        MAP.put(Rops.APUT_CHAR,            Dops.APUT_CHAR);
        MAP.put(Rops.APUT_SHORT,           Dops.APUT_SHORT);
        MAP.put(Rops.NEW_INSTANCE,         Dops.NEW_INSTANCE);
        MAP.put(Rops.CHECK_CAST,           Dops.CHECK_CAST);
        MAP.put(Rops.INSTANCE_OF,          Dops.INSTANCE_OF);

        MAP.put(Rops.GET_FIELD_LONG,       Dops.IGET_WIDE);
        MAP.put(Rops.GET_FIELD_FLOAT,      Dops.IGET);
        MAP.put(Rops.GET_FIELD_DOUBLE,     Dops.IGET_WIDE);
        MAP.put(Rops.GET_FIELD_OBJECT,     Dops.IGET_OBJECT);
        /*
         * Note: No map entries for get_field_* for non-long integral types,
         * since they need to be handled specially (see dopFor() below).
         */

        MAP.put(Rops.GET_STATIC_LONG,      Dops.SGET_WIDE);
        MAP.put(Rops.GET_STATIC_FLOAT,     Dops.SGET);
        MAP.put(Rops.GET_STATIC_DOUBLE,    Dops.SGET_WIDE);
        MAP.put(Rops.GET_STATIC_OBJECT,    Dops.SGET_OBJECT);
        /*
         * Note: No map entries for get_static* for non-long integral types,
         * since they need to be handled specially (see dopFor() below).
         */

        MAP.put(Rops.PUT_FIELD_LONG,       Dops.IPUT_WIDE);
        MAP.put(Rops.PUT_FIELD_FLOAT,      Dops.IPUT);
        MAP.put(Rops.PUT_FIELD_DOUBLE,     Dops.IPUT_WIDE);
        MAP.put(Rops.PUT_FIELD_OBJECT,     Dops.IPUT_OBJECT);
        /*
         * Note: No map entries for put_field_* for non-long integral types,
         * since they need to be handled specially (see dopFor() below).
         */

        MAP.put(Rops.PUT_STATIC_LONG,      Dops.SPUT_WIDE);
        MAP.put(Rops.PUT_STATIC_FLOAT,     Dops.SPUT);
        MAP.put(Rops.PUT_STATIC_DOUBLE,    Dops.SPUT_WIDE);
        MAP.put(Rops.PUT_STATIC_OBJECT,    Dops.SPUT_OBJECT);
        /*
         * Note: No map entries for put_static* for non-long integral types,
         * since they need to be handled specially (see dopFor() below).
         */

        /*
         * Note: No map entries for invoke*, new_array, and
         * filled_new_array, since they need to be handled specially
         * (see dopFor() below).
         */
    }

    /**
     * Returns the dalvik opcode appropriate for the given register-based
     * instruction.
     *
     * @param insn {@code non-null;} the original instruction
     * @return the corresponding dalvik opcode; one of the constants in
     * {@link Dops}
     */
    public static Dop dopFor(Insn insn) {
        Rop rop = insn.getOpcode();

        /*
         * First, just try looking up the rop in the MAP of easy
         * cases.
         */
        Dop result = MAP.get(rop);
        if (result != null) {
            return result;
        }

        /*
         * There was no easy case for the rop, so look up the opcode, and
         * do something special for each:
         *
         * The move_exception, new_array, filled_new_array, and
         * invoke* opcodes won't be found in MAP, since they'll each
         * have different source and/or result register types / lists.
         *
         * The get* and put* opcodes for (non-long) integral types
         * aren't in the map, since the type signatures aren't
         * sufficient to distinguish between the types (the salient
         * source or result will always be just "int").
         *
         * And const instruction need to distinguish between strings and
         * classes.
         */

        switch (rop.getOpcode()) {
            case RegOps.MOVE_EXCEPTION:   return Dops.MOVE_EXCEPTION;
            case RegOps.INVOKE_STATIC:    return Dops.INVOKE_STATIC;
            case RegOps.INVOKE_VIRTUAL:   return Dops.INVOKE_VIRTUAL;
            case RegOps.INVOKE_SUPER:     return Dops.INVOKE_SUPER;
            case RegOps.INVOKE_DIRECT:    return Dops.INVOKE_DIRECT;
            case RegOps.INVOKE_INTERFACE: return Dops.INVOKE_INTERFACE;
            case RegOps.NEW_ARRAY:        return Dops.NEW_ARRAY;
            case RegOps.FILLED_NEW_ARRAY: return Dops.FILLED_NEW_ARRAY;
            case RegOps.FILL_ARRAY_DATA:  return Dops.FILL_ARRAY_DATA;
            case RegOps.MOVE_RESULT: {
                RegisterSpec resultReg = insn.getResult();

                if (resultReg == null) {
                    return Dops.NOP;
                } else {
                    switch (resultReg.getBasicType()) {
                        case Type.BT_INT:
                        case Type.BT_FLOAT:
                        case Type.BT_BOOLEAN:
                        case Type.BT_BYTE:
                        case Type.BT_CHAR:
                        case Type.BT_SHORT:
                            return Dops.MOVE_RESULT;
                        case Type.BT_LONG:
                        case Type.BT_DOUBLE:
                            return Dops.MOVE_RESULT_WIDE;
                        case Type.BT_OBJECT:
                            return Dops.MOVE_RESULT_OBJECT;
                        default: {
                            throw new RuntimeException("Unexpected basic type");
                        }
                    }
                }
            }

            case RegOps.GET_FIELD: {
                CstFieldRef ref =
                    (CstFieldRef) ((ThrowingCstInsn) insn).getConstant();
                int basicType = ref.getBasicType();
                switch (basicType) {
                    case Type.BT_BOOLEAN: return Dops.IGET_BOOLEAN;
                    case Type.BT_BYTE:    return Dops.IGET_BYTE;
                    case Type.BT_CHAR:    return Dops.IGET_CHAR;
                    case Type.BT_SHORT:   return Dops.IGET_SHORT;
                    case Type.BT_INT:     return Dops.IGET;
                }
                break;
            }
            case RegOps.PUT_FIELD: {
                CstFieldRef ref =
                    (CstFieldRef) ((ThrowingCstInsn) insn).getConstant();
                int basicType = ref.getBasicType();
                switch (basicType) {
                    case Type.BT_BOOLEAN: return Dops.IPUT_BOOLEAN;
                    case Type.BT_BYTE:    return Dops.IPUT_BYTE;
                    case Type.BT_CHAR:    return Dops.IPUT_CHAR;
                    case Type.BT_SHORT:   return Dops.IPUT_SHORT;
                    case Type.BT_INT:     return Dops.IPUT;
                }
                break;
            }
            case RegOps.GET_STATIC: {
                CstFieldRef ref =
                    (CstFieldRef) ((ThrowingCstInsn) insn).getConstant();
                int basicType = ref.getBasicType();
                switch (basicType) {
                    case Type.BT_BOOLEAN: return Dops.SGET_BOOLEAN;
                    case Type.BT_BYTE:    return Dops.SGET_BYTE;
                    case Type.BT_CHAR:    return Dops.SGET_CHAR;
                    case Type.BT_SHORT:   return Dops.SGET_SHORT;
                    case Type.BT_INT:     return Dops.SGET;
                }
                break;
            }
            case RegOps.PUT_STATIC: {
                CstFieldRef ref =
                    (CstFieldRef) ((ThrowingCstInsn) insn).getConstant();
                int basicType = ref.getBasicType();
                switch (basicType) {
                    case Type.BT_BOOLEAN: return Dops.SPUT_BOOLEAN;
                    case Type.BT_BYTE:    return Dops.SPUT_BYTE;
                    case Type.BT_CHAR:    return Dops.SPUT_CHAR;
                    case Type.BT_SHORT:   return Dops.SPUT_SHORT;
                    case Type.BT_INT:     return Dops.SPUT;
                }
                break;
            }
            case RegOps.CONST: {
                Constant cst = ((ThrowingCstInsn) insn).getConstant();
                if (cst instanceof CstType) {
                    return Dops.CONST_CLASS;
                } else if (cst instanceof CstString) {
                    return Dops.CONST_STRING;
                }
                break;
            }
        }

        throw new RuntimeException("unknown rop: " + rop);
    }
}
