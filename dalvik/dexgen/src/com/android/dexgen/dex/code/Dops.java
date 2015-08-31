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

import com.android.dexgen.dex.code.form.Form10t;
import com.android.dexgen.dex.code.form.Form10x;
import com.android.dexgen.dex.code.form.Form11n;
import com.android.dexgen.dex.code.form.Form11x;
import com.android.dexgen.dex.code.form.Form12x;
import com.android.dexgen.dex.code.form.Form20t;
import com.android.dexgen.dex.code.form.Form21c;
import com.android.dexgen.dex.code.form.Form21h;
import com.android.dexgen.dex.code.form.Form21s;
import com.android.dexgen.dex.code.form.Form21t;
import com.android.dexgen.dex.code.form.Form22b;
import com.android.dexgen.dex.code.form.Form22c;
import com.android.dexgen.dex.code.form.Form22s;
import com.android.dexgen.dex.code.form.Form22t;
import com.android.dexgen.dex.code.form.Form22x;
import com.android.dexgen.dex.code.form.Form23x;
import com.android.dexgen.dex.code.form.Form30t;
import com.android.dexgen.dex.code.form.Form31c;
import com.android.dexgen.dex.code.form.Form31i;
import com.android.dexgen.dex.code.form.Form31t;
import com.android.dexgen.dex.code.form.Form32x;
import com.android.dexgen.dex.code.form.Form35c;
import com.android.dexgen.dex.code.form.Form3rc;
import com.android.dexgen.dex.code.form.Form51l;
import com.android.dexgen.dex.code.form.SpecialFormat;

/**
 * Standard instances of {@link Dop} and utility methods for getting
 * them.
 */
public final class Dops {
    /** {@code non-null;} array containing all the standard instances */
    private static final Dop[] DOPS;

    /**
     * pseudo-opcode used for nonstandard formatted "instructions"
     * (which are mostly not actually instructions, though they do
     * appear in instruction lists)
     */
    public static final Dop SPECIAL_FORMAT =
        new Dop(DalvOps.SPECIAL_FORMAT, DalvOps.SPECIAL_FORMAT,
                SpecialFormat.THE_ONE, false, "<special>");

    // BEGIN(dops); GENERATED AUTOMATICALLY BY opcode-gen
    public static final Dop NOP =
        new Dop(DalvOps.NOP, DalvOps.NOP,
            Form10x.THE_ONE, false, "nop");

    public static final Dop MOVE =
        new Dop(DalvOps.MOVE, DalvOps.MOVE,
            Form12x.THE_ONE, true, "move");

    public static final Dop MOVE_FROM16 =
        new Dop(DalvOps.MOVE_FROM16, DalvOps.MOVE,
            Form22x.THE_ONE, true, "move/from16");

    public static final Dop MOVE_16 =
        new Dop(DalvOps.MOVE_16, DalvOps.MOVE,
            Form32x.THE_ONE, true, "move/16");

    public static final Dop MOVE_WIDE =
        new Dop(DalvOps.MOVE_WIDE, DalvOps.MOVE_WIDE,
            Form12x.THE_ONE, true, "move-wide");

    public static final Dop MOVE_WIDE_FROM16 =
        new Dop(DalvOps.MOVE_WIDE_FROM16, DalvOps.MOVE_WIDE,
            Form22x.THE_ONE, true, "move-wide/from16");

    public static final Dop MOVE_WIDE_16 =
        new Dop(DalvOps.MOVE_WIDE_16, DalvOps.MOVE_WIDE,
            Form32x.THE_ONE, true, "move-wide/16");

    public static final Dop MOVE_OBJECT =
        new Dop(DalvOps.MOVE_OBJECT, DalvOps.MOVE_OBJECT,
            Form12x.THE_ONE, true, "move-object");

    public static final Dop MOVE_OBJECT_FROM16 =
        new Dop(DalvOps.MOVE_OBJECT_FROM16, DalvOps.MOVE_OBJECT,
            Form22x.THE_ONE, true, "move-object/from16");

    public static final Dop MOVE_OBJECT_16 =
        new Dop(DalvOps.MOVE_OBJECT_16, DalvOps.MOVE_OBJECT,
            Form32x.THE_ONE, true, "move-object/16");

    public static final Dop MOVE_RESULT =
        new Dop(DalvOps.MOVE_RESULT, DalvOps.MOVE_RESULT,
            Form11x.THE_ONE, true, "move-result");

    public static final Dop MOVE_RESULT_WIDE =
        new Dop(DalvOps.MOVE_RESULT_WIDE, DalvOps.MOVE_RESULT_WIDE,
            Form11x.THE_ONE, true, "move-result-wide");

    public static final Dop MOVE_RESULT_OBJECT =
        new Dop(DalvOps.MOVE_RESULT_OBJECT, DalvOps.MOVE_RESULT_OBJECT,
            Form11x.THE_ONE, true, "move-result-object");

    public static final Dop MOVE_EXCEPTION =
        new Dop(DalvOps.MOVE_EXCEPTION, DalvOps.MOVE_EXCEPTION,
            Form11x.THE_ONE, true, "move-exception");

    public static final Dop RETURN_VOID =
        new Dop(DalvOps.RETURN_VOID, DalvOps.RETURN_VOID,
            Form10x.THE_ONE, false, "return-void");

    public static final Dop RETURN =
        new Dop(DalvOps.RETURN, DalvOps.RETURN,
            Form11x.THE_ONE, false, "return");

    public static final Dop RETURN_WIDE =
        new Dop(DalvOps.RETURN_WIDE, DalvOps.RETURN_WIDE,
            Form11x.THE_ONE, false, "return-wide");

    public static final Dop RETURN_OBJECT =
        new Dop(DalvOps.RETURN_OBJECT, DalvOps.RETURN_OBJECT,
            Form11x.THE_ONE, false, "return-object");

    public static final Dop CONST_4 =
        new Dop(DalvOps.CONST_4, DalvOps.CONST,
            Form11n.THE_ONE, true, "const/4");

    public static final Dop CONST_16 =
        new Dop(DalvOps.CONST_16, DalvOps.CONST,
            Form21s.THE_ONE, true, "const/16");

    public static final Dop CONST =
        new Dop(DalvOps.CONST, DalvOps.CONST,
            Form31i.THE_ONE, true, "const");

    public static final Dop CONST_HIGH16 =
        new Dop(DalvOps.CONST_HIGH16, DalvOps.CONST,
            Form21h.THE_ONE, true, "const/high16");

    public static final Dop CONST_WIDE_16 =
        new Dop(DalvOps.CONST_WIDE_16, DalvOps.CONST_WIDE,
            Form21s.THE_ONE, true, "const-wide/16");

    public static final Dop CONST_WIDE_32 =
        new Dop(DalvOps.CONST_WIDE_32, DalvOps.CONST_WIDE,
            Form31i.THE_ONE, true, "const-wide/32");

    public static final Dop CONST_WIDE =
        new Dop(DalvOps.CONST_WIDE, DalvOps.CONST_WIDE,
            Form51l.THE_ONE, true, "const-wide");

    public static final Dop CONST_WIDE_HIGH16 =
        new Dop(DalvOps.CONST_WIDE_HIGH16, DalvOps.CONST_WIDE,
            Form21h.THE_ONE, true, "const-wide/high16");

    public static final Dop CONST_STRING =
        new Dop(DalvOps.CONST_STRING, DalvOps.CONST_STRING,
            Form21c.THE_ONE, true, "const-string");

    public static final Dop CONST_STRING_JUMBO =
        new Dop(DalvOps.CONST_STRING_JUMBO, DalvOps.CONST_STRING,
            Form31c.THE_ONE, true, "const-string/jumbo");

    public static final Dop CONST_CLASS =
        new Dop(DalvOps.CONST_CLASS, DalvOps.CONST_CLASS,
            Form21c.THE_ONE, true, "const-class");

    public static final Dop MONITOR_ENTER =
        new Dop(DalvOps.MONITOR_ENTER, DalvOps.MONITOR_ENTER,
            Form11x.THE_ONE, false, "monitor-enter");

    public static final Dop MONITOR_EXIT =
        new Dop(DalvOps.MONITOR_EXIT, DalvOps.MONITOR_EXIT,
            Form11x.THE_ONE, false, "monitor-exit");

    public static final Dop CHECK_CAST =
        new Dop(DalvOps.CHECK_CAST, DalvOps.CHECK_CAST,
            Form21c.THE_ONE, true, "check-cast");

    public static final Dop INSTANCE_OF =
        new Dop(DalvOps.INSTANCE_OF, DalvOps.INSTANCE_OF,
            Form22c.THE_ONE, true, "instance-of");

    public static final Dop ARRAY_LENGTH =
        new Dop(DalvOps.ARRAY_LENGTH, DalvOps.ARRAY_LENGTH,
            Form12x.THE_ONE, true, "array-length");

    public static final Dop NEW_INSTANCE =
        new Dop(DalvOps.NEW_INSTANCE, DalvOps.NEW_INSTANCE,
            Form21c.THE_ONE, true, "new-instance");

    public static final Dop NEW_ARRAY =
        new Dop(DalvOps.NEW_ARRAY, DalvOps.NEW_ARRAY,
            Form22c.THE_ONE, true, "new-array");

    public static final Dop FILLED_NEW_ARRAY =
        new Dop(DalvOps.FILLED_NEW_ARRAY, DalvOps.FILLED_NEW_ARRAY,
            Form35c.THE_ONE, false, "filled-new-array");

    public static final Dop FILLED_NEW_ARRAY_RANGE =
        new Dop(DalvOps.FILLED_NEW_ARRAY_RANGE, DalvOps.FILLED_NEW_ARRAY,
            Form3rc.THE_ONE, false, "filled-new-array/range");

    public static final Dop FILL_ARRAY_DATA =
        new Dop(DalvOps.FILL_ARRAY_DATA, DalvOps.FILL_ARRAY_DATA,
            Form31t.THE_ONE, false, "fill-array-data");

    public static final Dop THROW =
        new Dop(DalvOps.THROW, DalvOps.THROW,
            Form11x.THE_ONE, false, "throw");

    public static final Dop GOTO =
        new Dop(DalvOps.GOTO, DalvOps.GOTO,
            Form10t.THE_ONE, false, "goto");

    public static final Dop GOTO_16 =
        new Dop(DalvOps.GOTO_16, DalvOps.GOTO,
            Form20t.THE_ONE, false, "goto/16");

    public static final Dop GOTO_32 =
        new Dop(DalvOps.GOTO_32, DalvOps.GOTO,
            Form30t.THE_ONE, false, "goto/32");

    public static final Dop PACKED_SWITCH =
        new Dop(DalvOps.PACKED_SWITCH, DalvOps.PACKED_SWITCH,
            Form31t.THE_ONE, false, "packed-switch");

    public static final Dop SPARSE_SWITCH =
        new Dop(DalvOps.SPARSE_SWITCH, DalvOps.SPARSE_SWITCH,
            Form31t.THE_ONE, false, "sparse-switch");

    public static final Dop CMPL_FLOAT =
        new Dop(DalvOps.CMPL_FLOAT, DalvOps.CMPL_FLOAT,
            Form23x.THE_ONE, true, "cmpl-float");

    public static final Dop CMPG_FLOAT =
        new Dop(DalvOps.CMPG_FLOAT, DalvOps.CMPG_FLOAT,
            Form23x.THE_ONE, true, "cmpg-float");

    public static final Dop CMPL_DOUBLE =
        new Dop(DalvOps.CMPL_DOUBLE, DalvOps.CMPL_DOUBLE,
            Form23x.THE_ONE, true, "cmpl-double");

    public static final Dop CMPG_DOUBLE =
        new Dop(DalvOps.CMPG_DOUBLE, DalvOps.CMPG_DOUBLE,
            Form23x.THE_ONE, true, "cmpg-double");

    public static final Dop CMP_LONG =
        new Dop(DalvOps.CMP_LONG, DalvOps.CMP_LONG,
            Form23x.THE_ONE, true, "cmp-long");

    public static final Dop IF_EQ =
        new Dop(DalvOps.IF_EQ, DalvOps.IF_EQ,
            Form22t.THE_ONE, false, "if-eq");

    public static final Dop IF_NE =
        new Dop(DalvOps.IF_NE, DalvOps.IF_NE,
            Form22t.THE_ONE, false, "if-ne");

    public static final Dop IF_LT =
        new Dop(DalvOps.IF_LT, DalvOps.IF_LT,
            Form22t.THE_ONE, false, "if-lt");

    public static final Dop IF_GE =
        new Dop(DalvOps.IF_GE, DalvOps.IF_GE,
            Form22t.THE_ONE, false, "if-ge");

    public static final Dop IF_GT =
        new Dop(DalvOps.IF_GT, DalvOps.IF_GT,
            Form22t.THE_ONE, false, "if-gt");

    public static final Dop IF_LE =
        new Dop(DalvOps.IF_LE, DalvOps.IF_LE,
            Form22t.THE_ONE, false, "if-le");

    public static final Dop IF_EQZ =
        new Dop(DalvOps.IF_EQZ, DalvOps.IF_EQZ,
            Form21t.THE_ONE, false, "if-eqz");

    public static final Dop IF_NEZ =
        new Dop(DalvOps.IF_NEZ, DalvOps.IF_NEZ,
            Form21t.THE_ONE, false, "if-nez");

    public static final Dop IF_LTZ =
        new Dop(DalvOps.IF_LTZ, DalvOps.IF_LTZ,
            Form21t.THE_ONE, false, "if-ltz");

    public static final Dop IF_GEZ =
        new Dop(DalvOps.IF_GEZ, DalvOps.IF_GEZ,
            Form21t.THE_ONE, false, "if-gez");

    public static final Dop IF_GTZ =
        new Dop(DalvOps.IF_GTZ, DalvOps.IF_GTZ,
            Form21t.THE_ONE, false, "if-gtz");

    public static final Dop IF_LEZ =
        new Dop(DalvOps.IF_LEZ, DalvOps.IF_LEZ,
            Form21t.THE_ONE, false, "if-lez");

    public static final Dop AGET =
        new Dop(DalvOps.AGET, DalvOps.AGET,
            Form23x.THE_ONE, true, "aget");

    public static final Dop AGET_WIDE =
        new Dop(DalvOps.AGET_WIDE, DalvOps.AGET_WIDE,
            Form23x.THE_ONE, true, "aget-wide");

    public static final Dop AGET_OBJECT =
        new Dop(DalvOps.AGET_OBJECT, DalvOps.AGET_OBJECT,
            Form23x.THE_ONE, true, "aget-object");

    public static final Dop AGET_BOOLEAN =
        new Dop(DalvOps.AGET_BOOLEAN, DalvOps.AGET_BOOLEAN,
            Form23x.THE_ONE, true, "aget-boolean");

    public static final Dop AGET_BYTE =
        new Dop(DalvOps.AGET_BYTE, DalvOps.AGET_BYTE,
            Form23x.THE_ONE, true, "aget-byte");

    public static final Dop AGET_CHAR =
        new Dop(DalvOps.AGET_CHAR, DalvOps.AGET_CHAR,
            Form23x.THE_ONE, true, "aget-char");

    public static final Dop AGET_SHORT =
        new Dop(DalvOps.AGET_SHORT, DalvOps.AGET_SHORT,
            Form23x.THE_ONE, true, "aget-short");

    public static final Dop APUT =
        new Dop(DalvOps.APUT, DalvOps.APUT,
            Form23x.THE_ONE, false, "aput");

    public static final Dop APUT_WIDE =
        new Dop(DalvOps.APUT_WIDE, DalvOps.APUT_WIDE,
            Form23x.THE_ONE, false, "aput-wide");

    public static final Dop APUT_OBJECT =
        new Dop(DalvOps.APUT_OBJECT, DalvOps.APUT_OBJECT,
            Form23x.THE_ONE, false, "aput-object");

    public static final Dop APUT_BOOLEAN =
        new Dop(DalvOps.APUT_BOOLEAN, DalvOps.APUT_BOOLEAN,
            Form23x.THE_ONE, false, "aput-boolean");

    public static final Dop APUT_BYTE =
        new Dop(DalvOps.APUT_BYTE, DalvOps.APUT_BYTE,
            Form23x.THE_ONE, false, "aput-byte");

    public static final Dop APUT_CHAR =
        new Dop(DalvOps.APUT_CHAR, DalvOps.APUT_CHAR,
            Form23x.THE_ONE, false, "aput-char");

    public static final Dop APUT_SHORT =
        new Dop(DalvOps.APUT_SHORT, DalvOps.APUT_SHORT,
            Form23x.THE_ONE, false, "aput-short");

    public static final Dop IGET =
        new Dop(DalvOps.IGET, DalvOps.IGET,
            Form22c.THE_ONE, true, "iget");

    public static final Dop IGET_WIDE =
        new Dop(DalvOps.IGET_WIDE, DalvOps.IGET_WIDE,
            Form22c.THE_ONE, true, "iget-wide");

    public static final Dop IGET_OBJECT =
        new Dop(DalvOps.IGET_OBJECT, DalvOps.IGET_OBJECT,
            Form22c.THE_ONE, true, "iget-object");

    public static final Dop IGET_BOOLEAN =
        new Dop(DalvOps.IGET_BOOLEAN, DalvOps.IGET_BOOLEAN,
            Form22c.THE_ONE, true, "iget-boolean");

    public static final Dop IGET_BYTE =
        new Dop(DalvOps.IGET_BYTE, DalvOps.IGET_BYTE,
            Form22c.THE_ONE, true, "iget-byte");

    public static final Dop IGET_CHAR =
        new Dop(DalvOps.IGET_CHAR, DalvOps.IGET_CHAR,
            Form22c.THE_ONE, true, "iget-char");

    public static final Dop IGET_SHORT =
        new Dop(DalvOps.IGET_SHORT, DalvOps.IGET_SHORT,
            Form22c.THE_ONE, true, "iget-short");

    public static final Dop IPUT =
        new Dop(DalvOps.IPUT, DalvOps.IPUT,
            Form22c.THE_ONE, false, "iput");

    public static final Dop IPUT_WIDE =
        new Dop(DalvOps.IPUT_WIDE, DalvOps.IPUT_WIDE,
            Form22c.THE_ONE, false, "iput-wide");

    public static final Dop IPUT_OBJECT =
        new Dop(DalvOps.IPUT_OBJECT, DalvOps.IPUT_OBJECT,
            Form22c.THE_ONE, false, "iput-object");

    public static final Dop IPUT_BOOLEAN =
        new Dop(DalvOps.IPUT_BOOLEAN, DalvOps.IPUT_BOOLEAN,
            Form22c.THE_ONE, false, "iput-boolean");

    public static final Dop IPUT_BYTE =
        new Dop(DalvOps.IPUT_BYTE, DalvOps.IPUT_BYTE,
            Form22c.THE_ONE, false, "iput-byte");

    public static final Dop IPUT_CHAR =
        new Dop(DalvOps.IPUT_CHAR, DalvOps.IPUT_CHAR,
            Form22c.THE_ONE, false, "iput-char");

    public static final Dop IPUT_SHORT =
        new Dop(DalvOps.IPUT_SHORT, DalvOps.IPUT_SHORT,
            Form22c.THE_ONE, false, "iput-short");

    public static final Dop SGET =
        new Dop(DalvOps.SGET, DalvOps.SGET,
            Form21c.THE_ONE, true, "sget");

    public static final Dop SGET_WIDE =
        new Dop(DalvOps.SGET_WIDE, DalvOps.SGET_WIDE,
            Form21c.THE_ONE, true, "sget-wide");

    public static final Dop SGET_OBJECT =
        new Dop(DalvOps.SGET_OBJECT, DalvOps.SGET_OBJECT,
            Form21c.THE_ONE, true, "sget-object");

    public static final Dop SGET_BOOLEAN =
        new Dop(DalvOps.SGET_BOOLEAN, DalvOps.SGET_BOOLEAN,
            Form21c.THE_ONE, true, "sget-boolean");

    public static final Dop SGET_BYTE =
        new Dop(DalvOps.SGET_BYTE, DalvOps.SGET_BYTE,
            Form21c.THE_ONE, true, "sget-byte");

    public static final Dop SGET_CHAR =
        new Dop(DalvOps.SGET_CHAR, DalvOps.SGET_CHAR,
            Form21c.THE_ONE, true, "sget-char");

    public static final Dop SGET_SHORT =
        new Dop(DalvOps.SGET_SHORT, DalvOps.SGET_SHORT,
            Form21c.THE_ONE, true, "sget-short");

    public static final Dop SPUT =
        new Dop(DalvOps.SPUT, DalvOps.SPUT,
            Form21c.THE_ONE, false, "sput");

    public static final Dop SPUT_WIDE =
        new Dop(DalvOps.SPUT_WIDE, DalvOps.SPUT_WIDE,
            Form21c.THE_ONE, false, "sput-wide");

    public static final Dop SPUT_OBJECT =
        new Dop(DalvOps.SPUT_OBJECT, DalvOps.SPUT_OBJECT,
            Form21c.THE_ONE, false, "sput-object");

    public static final Dop SPUT_BOOLEAN =
        new Dop(DalvOps.SPUT_BOOLEAN, DalvOps.SPUT_BOOLEAN,
            Form21c.THE_ONE, false, "sput-boolean");

    public static final Dop SPUT_BYTE =
        new Dop(DalvOps.SPUT_BYTE, DalvOps.SPUT_BYTE,
            Form21c.THE_ONE, false, "sput-byte");

    public static final Dop SPUT_CHAR =
        new Dop(DalvOps.SPUT_CHAR, DalvOps.SPUT_CHAR,
            Form21c.THE_ONE, false, "sput-char");

    public static final Dop SPUT_SHORT =
        new Dop(DalvOps.SPUT_SHORT, DalvOps.SPUT_SHORT,
            Form21c.THE_ONE, false, "sput-short");

    public static final Dop INVOKE_VIRTUAL =
        new Dop(DalvOps.INVOKE_VIRTUAL, DalvOps.INVOKE_VIRTUAL,
            Form35c.THE_ONE, false, "invoke-virtual");

    public static final Dop INVOKE_SUPER =
        new Dop(DalvOps.INVOKE_SUPER, DalvOps.INVOKE_SUPER,
            Form35c.THE_ONE, false, "invoke-super");

    public static final Dop INVOKE_DIRECT =
        new Dop(DalvOps.INVOKE_DIRECT, DalvOps.INVOKE_DIRECT,
            Form35c.THE_ONE, false, "invoke-direct");

    public static final Dop INVOKE_STATIC =
        new Dop(DalvOps.INVOKE_STATIC, DalvOps.INVOKE_STATIC,
            Form35c.THE_ONE, false, "invoke-static");

    public static final Dop INVOKE_INTERFACE =
        new Dop(DalvOps.INVOKE_INTERFACE, DalvOps.INVOKE_INTERFACE,
            Form35c.THE_ONE, false, "invoke-interface");

    public static final Dop INVOKE_VIRTUAL_RANGE =
        new Dop(DalvOps.INVOKE_VIRTUAL_RANGE, DalvOps.INVOKE_VIRTUAL,
            Form3rc.THE_ONE, false, "invoke-virtual/range");

    public static final Dop INVOKE_SUPER_RANGE =
        new Dop(DalvOps.INVOKE_SUPER_RANGE, DalvOps.INVOKE_SUPER,
            Form3rc.THE_ONE, false, "invoke-super/range");

    public static final Dop INVOKE_DIRECT_RANGE =
        new Dop(DalvOps.INVOKE_DIRECT_RANGE, DalvOps.INVOKE_DIRECT,
            Form3rc.THE_ONE, false, "invoke-direct/range");

    public static final Dop INVOKE_STATIC_RANGE =
        new Dop(DalvOps.INVOKE_STATIC_RANGE, DalvOps.INVOKE_STATIC,
            Form3rc.THE_ONE, false, "invoke-static/range");

    public static final Dop INVOKE_INTERFACE_RANGE =
        new Dop(DalvOps.INVOKE_INTERFACE_RANGE, DalvOps.INVOKE_INTERFACE,
            Form3rc.THE_ONE, false, "invoke-interface/range");

    public static final Dop NEG_INT =
        new Dop(DalvOps.NEG_INT, DalvOps.NEG_INT,
            Form12x.THE_ONE, true, "neg-int");

    public static final Dop NOT_INT =
        new Dop(DalvOps.NOT_INT, DalvOps.NOT_INT,
            Form12x.THE_ONE, true, "not-int");

    public static final Dop NEG_LONG =
        new Dop(DalvOps.NEG_LONG, DalvOps.NEG_LONG,
            Form12x.THE_ONE, true, "neg-long");

    public static final Dop NOT_LONG =
        new Dop(DalvOps.NOT_LONG, DalvOps.NOT_LONG,
            Form12x.THE_ONE, true, "not-long");

    public static final Dop NEG_FLOAT =
        new Dop(DalvOps.NEG_FLOAT, DalvOps.NEG_FLOAT,
            Form12x.THE_ONE, true, "neg-float");

    public static final Dop NEG_DOUBLE =
        new Dop(DalvOps.NEG_DOUBLE, DalvOps.NEG_DOUBLE,
            Form12x.THE_ONE, true, "neg-double");

    public static final Dop INT_TO_LONG =
        new Dop(DalvOps.INT_TO_LONG, DalvOps.INT_TO_LONG,
            Form12x.THE_ONE, true, "int-to-long");

    public static final Dop INT_TO_FLOAT =
        new Dop(DalvOps.INT_TO_FLOAT, DalvOps.INT_TO_FLOAT,
            Form12x.THE_ONE, true, "int-to-float");

    public static final Dop INT_TO_DOUBLE =
        new Dop(DalvOps.INT_TO_DOUBLE, DalvOps.INT_TO_DOUBLE,
            Form12x.THE_ONE, true, "int-to-double");

    public static final Dop LONG_TO_INT =
        new Dop(DalvOps.LONG_TO_INT, DalvOps.LONG_TO_INT,
            Form12x.THE_ONE, true, "long-to-int");

    public static final Dop LONG_TO_FLOAT =
        new Dop(DalvOps.LONG_TO_FLOAT, DalvOps.LONG_TO_FLOAT,
            Form12x.THE_ONE, true, "long-to-float");

    public static final Dop LONG_TO_DOUBLE =
        new Dop(DalvOps.LONG_TO_DOUBLE, DalvOps.LONG_TO_DOUBLE,
            Form12x.THE_ONE, true, "long-to-double");

    public static final Dop FLOAT_TO_INT =
        new Dop(DalvOps.FLOAT_TO_INT, DalvOps.FLOAT_TO_INT,
            Form12x.THE_ONE, true, "float-to-int");

    public static final Dop FLOAT_TO_LONG =
        new Dop(DalvOps.FLOAT_TO_LONG, DalvOps.FLOAT_TO_LONG,
            Form12x.THE_ONE, true, "float-to-long");

    public static final Dop FLOAT_TO_DOUBLE =
        new Dop(DalvOps.FLOAT_TO_DOUBLE, DalvOps.FLOAT_TO_DOUBLE,
            Form12x.THE_ONE, true, "float-to-double");

    public static final Dop DOUBLE_TO_INT =
        new Dop(DalvOps.DOUBLE_TO_INT, DalvOps.DOUBLE_TO_INT,
            Form12x.THE_ONE, true, "double-to-int");

    public static final Dop DOUBLE_TO_LONG =
        new Dop(DalvOps.DOUBLE_TO_LONG, DalvOps.DOUBLE_TO_LONG,
            Form12x.THE_ONE, true, "double-to-long");

    public static final Dop DOUBLE_TO_FLOAT =
        new Dop(DalvOps.DOUBLE_TO_FLOAT, DalvOps.DOUBLE_TO_FLOAT,
            Form12x.THE_ONE, true, "double-to-float");

    public static final Dop INT_TO_BYTE =
        new Dop(DalvOps.INT_TO_BYTE, DalvOps.INT_TO_BYTE,
            Form12x.THE_ONE, true, "int-to-byte");

    public static final Dop INT_TO_CHAR =
        new Dop(DalvOps.INT_TO_CHAR, DalvOps.INT_TO_CHAR,
            Form12x.THE_ONE, true, "int-to-char");

    public static final Dop INT_TO_SHORT =
        new Dop(DalvOps.INT_TO_SHORT, DalvOps.INT_TO_SHORT,
            Form12x.THE_ONE, true, "int-to-short");

    public static final Dop ADD_INT =
        new Dop(DalvOps.ADD_INT, DalvOps.ADD_INT,
            Form23x.THE_ONE, true, "add-int");

    public static final Dop SUB_INT =
        new Dop(DalvOps.SUB_INT, DalvOps.SUB_INT,
            Form23x.THE_ONE, true, "sub-int");

    public static final Dop MUL_INT =
        new Dop(DalvOps.MUL_INT, DalvOps.MUL_INT,
            Form23x.THE_ONE, true, "mul-int");

    public static final Dop DIV_INT =
        new Dop(DalvOps.DIV_INT, DalvOps.DIV_INT,
            Form23x.THE_ONE, true, "div-int");

    public static final Dop REM_INT =
        new Dop(DalvOps.REM_INT, DalvOps.REM_INT,
            Form23x.THE_ONE, true, "rem-int");

    public static final Dop AND_INT =
        new Dop(DalvOps.AND_INT, DalvOps.AND_INT,
            Form23x.THE_ONE, true, "and-int");

    public static final Dop OR_INT =
        new Dop(DalvOps.OR_INT, DalvOps.OR_INT,
            Form23x.THE_ONE, true, "or-int");

    public static final Dop XOR_INT =
        new Dop(DalvOps.XOR_INT, DalvOps.XOR_INT,
            Form23x.THE_ONE, true, "xor-int");

    public static final Dop SHL_INT =
        new Dop(DalvOps.SHL_INT, DalvOps.SHL_INT,
            Form23x.THE_ONE, true, "shl-int");

    public static final Dop SHR_INT =
        new Dop(DalvOps.SHR_INT, DalvOps.SHR_INT,
            Form23x.THE_ONE, true, "shr-int");

    public static final Dop USHR_INT =
        new Dop(DalvOps.USHR_INT, DalvOps.USHR_INT,
            Form23x.THE_ONE, true, "ushr-int");

    public static final Dop ADD_LONG =
        new Dop(DalvOps.ADD_LONG, DalvOps.ADD_LONG,
            Form23x.THE_ONE, true, "add-long");

    public static final Dop SUB_LONG =
        new Dop(DalvOps.SUB_LONG, DalvOps.SUB_LONG,
            Form23x.THE_ONE, true, "sub-long");

    public static final Dop MUL_LONG =
        new Dop(DalvOps.MUL_LONG, DalvOps.MUL_LONG,
            Form23x.THE_ONE, true, "mul-long");

    public static final Dop DIV_LONG =
        new Dop(DalvOps.DIV_LONG, DalvOps.DIV_LONG,
            Form23x.THE_ONE, true, "div-long");

    public static final Dop REM_LONG =
        new Dop(DalvOps.REM_LONG, DalvOps.REM_LONG,
            Form23x.THE_ONE, true, "rem-long");

    public static final Dop AND_LONG =
        new Dop(DalvOps.AND_LONG, DalvOps.AND_LONG,
            Form23x.THE_ONE, true, "and-long");

    public static final Dop OR_LONG =
        new Dop(DalvOps.OR_LONG, DalvOps.OR_LONG,
            Form23x.THE_ONE, true, "or-long");

    public static final Dop XOR_LONG =
        new Dop(DalvOps.XOR_LONG, DalvOps.XOR_LONG,
            Form23x.THE_ONE, true, "xor-long");

    public static final Dop SHL_LONG =
        new Dop(DalvOps.SHL_LONG, DalvOps.SHL_LONG,
            Form23x.THE_ONE, true, "shl-long");

    public static final Dop SHR_LONG =
        new Dop(DalvOps.SHR_LONG, DalvOps.SHR_LONG,
            Form23x.THE_ONE, true, "shr-long");

    public static final Dop USHR_LONG =
        new Dop(DalvOps.USHR_LONG, DalvOps.USHR_LONG,
            Form23x.THE_ONE, true, "ushr-long");

    public static final Dop ADD_FLOAT =
        new Dop(DalvOps.ADD_FLOAT, DalvOps.ADD_FLOAT,
            Form23x.THE_ONE, true, "add-float");

    public static final Dop SUB_FLOAT =
        new Dop(DalvOps.SUB_FLOAT, DalvOps.SUB_FLOAT,
            Form23x.THE_ONE, true, "sub-float");

    public static final Dop MUL_FLOAT =
        new Dop(DalvOps.MUL_FLOAT, DalvOps.MUL_FLOAT,
            Form23x.THE_ONE, true, "mul-float");

    public static final Dop DIV_FLOAT =
        new Dop(DalvOps.DIV_FLOAT, DalvOps.DIV_FLOAT,
            Form23x.THE_ONE, true, "div-float");

    public static final Dop REM_FLOAT =
        new Dop(DalvOps.REM_FLOAT, DalvOps.REM_FLOAT,
            Form23x.THE_ONE, true, "rem-float");

    public static final Dop ADD_DOUBLE =
        new Dop(DalvOps.ADD_DOUBLE, DalvOps.ADD_DOUBLE,
            Form23x.THE_ONE, true, "add-double");

    public static final Dop SUB_DOUBLE =
        new Dop(DalvOps.SUB_DOUBLE, DalvOps.SUB_DOUBLE,
            Form23x.THE_ONE, true, "sub-double");

    public static final Dop MUL_DOUBLE =
        new Dop(DalvOps.MUL_DOUBLE, DalvOps.MUL_DOUBLE,
            Form23x.THE_ONE, true, "mul-double");

    public static final Dop DIV_DOUBLE =
        new Dop(DalvOps.DIV_DOUBLE, DalvOps.DIV_DOUBLE,
            Form23x.THE_ONE, true, "div-double");

    public static final Dop REM_DOUBLE =
        new Dop(DalvOps.REM_DOUBLE, DalvOps.REM_DOUBLE,
            Form23x.THE_ONE, true, "rem-double");

    public static final Dop ADD_INT_2ADDR =
        new Dop(DalvOps.ADD_INT_2ADDR, DalvOps.ADD_INT,
            Form12x.THE_ONE, true, "add-int/2addr");

    public static final Dop SUB_INT_2ADDR =
        new Dop(DalvOps.SUB_INT_2ADDR, DalvOps.SUB_INT,
            Form12x.THE_ONE, true, "sub-int/2addr");

    public static final Dop MUL_INT_2ADDR =
        new Dop(DalvOps.MUL_INT_2ADDR, DalvOps.MUL_INT,
            Form12x.THE_ONE, true, "mul-int/2addr");

    public static final Dop DIV_INT_2ADDR =
        new Dop(DalvOps.DIV_INT_2ADDR, DalvOps.DIV_INT,
            Form12x.THE_ONE, true, "div-int/2addr");

    public static final Dop REM_INT_2ADDR =
        new Dop(DalvOps.REM_INT_2ADDR, DalvOps.REM_INT,
            Form12x.THE_ONE, true, "rem-int/2addr");

    public static final Dop AND_INT_2ADDR =
        new Dop(DalvOps.AND_INT_2ADDR, DalvOps.AND_INT,
            Form12x.THE_ONE, true, "and-int/2addr");

    public static final Dop OR_INT_2ADDR =
        new Dop(DalvOps.OR_INT_2ADDR, DalvOps.OR_INT,
            Form12x.THE_ONE, true, "or-int/2addr");

    public static final Dop XOR_INT_2ADDR =
        new Dop(DalvOps.XOR_INT_2ADDR, DalvOps.XOR_INT,
            Form12x.THE_ONE, true, "xor-int/2addr");

    public static final Dop SHL_INT_2ADDR =
        new Dop(DalvOps.SHL_INT_2ADDR, DalvOps.SHL_INT,
            Form12x.THE_ONE, true, "shl-int/2addr");

    public static final Dop SHR_INT_2ADDR =
        new Dop(DalvOps.SHR_INT_2ADDR, DalvOps.SHR_INT,
            Form12x.THE_ONE, true, "shr-int/2addr");

    public static final Dop USHR_INT_2ADDR =
        new Dop(DalvOps.USHR_INT_2ADDR, DalvOps.USHR_INT,
            Form12x.THE_ONE, true, "ushr-int/2addr");

    public static final Dop ADD_LONG_2ADDR =
        new Dop(DalvOps.ADD_LONG_2ADDR, DalvOps.ADD_LONG,
            Form12x.THE_ONE, true, "add-long/2addr");

    public static final Dop SUB_LONG_2ADDR =
        new Dop(DalvOps.SUB_LONG_2ADDR, DalvOps.SUB_LONG,
            Form12x.THE_ONE, true, "sub-long/2addr");

    public static final Dop MUL_LONG_2ADDR =
        new Dop(DalvOps.MUL_LONG_2ADDR, DalvOps.MUL_LONG,
            Form12x.THE_ONE, true, "mul-long/2addr");

    public static final Dop DIV_LONG_2ADDR =
        new Dop(DalvOps.DIV_LONG_2ADDR, DalvOps.DIV_LONG,
            Form12x.THE_ONE, true, "div-long/2addr");

    public static final Dop REM_LONG_2ADDR =
        new Dop(DalvOps.REM_LONG_2ADDR, DalvOps.REM_LONG,
            Form12x.THE_ONE, true, "rem-long/2addr");

    public static final Dop AND_LONG_2ADDR =
        new Dop(DalvOps.AND_LONG_2ADDR, DalvOps.AND_LONG,
            Form12x.THE_ONE, true, "and-long/2addr");

    public static final Dop OR_LONG_2ADDR =
        new Dop(DalvOps.OR_LONG_2ADDR, DalvOps.OR_LONG,
            Form12x.THE_ONE, true, "or-long/2addr");

    public static final Dop XOR_LONG_2ADDR =
        new Dop(DalvOps.XOR_LONG_2ADDR, DalvOps.XOR_LONG,
            Form12x.THE_ONE, true, "xor-long/2addr");

    public static final Dop SHL_LONG_2ADDR =
        new Dop(DalvOps.SHL_LONG_2ADDR, DalvOps.SHL_LONG,
            Form12x.THE_ONE, true, "shl-long/2addr");

    public static final Dop SHR_LONG_2ADDR =
        new Dop(DalvOps.SHR_LONG_2ADDR, DalvOps.SHR_LONG,
            Form12x.THE_ONE, true, "shr-long/2addr");

    public static final Dop USHR_LONG_2ADDR =
        new Dop(DalvOps.USHR_LONG_2ADDR, DalvOps.USHR_LONG,
            Form12x.THE_ONE, true, "ushr-long/2addr");

    public static final Dop ADD_FLOAT_2ADDR =
        new Dop(DalvOps.ADD_FLOAT_2ADDR, DalvOps.ADD_FLOAT,
            Form12x.THE_ONE, true, "add-float/2addr");

    public static final Dop SUB_FLOAT_2ADDR =
        new Dop(DalvOps.SUB_FLOAT_2ADDR, DalvOps.SUB_FLOAT,
            Form12x.THE_ONE, true, "sub-float/2addr");

    public static final Dop MUL_FLOAT_2ADDR =
        new Dop(DalvOps.MUL_FLOAT_2ADDR, DalvOps.MUL_FLOAT,
            Form12x.THE_ONE, true, "mul-float/2addr");

    public static final Dop DIV_FLOAT_2ADDR =
        new Dop(DalvOps.DIV_FLOAT_2ADDR, DalvOps.DIV_FLOAT,
            Form12x.THE_ONE, true, "div-float/2addr");

    public static final Dop REM_FLOAT_2ADDR =
        new Dop(DalvOps.REM_FLOAT_2ADDR, DalvOps.REM_FLOAT,
            Form12x.THE_ONE, true, "rem-float/2addr");

    public static final Dop ADD_DOUBLE_2ADDR =
        new Dop(DalvOps.ADD_DOUBLE_2ADDR, DalvOps.ADD_DOUBLE,
            Form12x.THE_ONE, true, "add-double/2addr");

    public static final Dop SUB_DOUBLE_2ADDR =
        new Dop(DalvOps.SUB_DOUBLE_2ADDR, DalvOps.SUB_DOUBLE,
            Form12x.THE_ONE, true, "sub-double/2addr");

    public static final Dop MUL_DOUBLE_2ADDR =
        new Dop(DalvOps.MUL_DOUBLE_2ADDR, DalvOps.MUL_DOUBLE,
            Form12x.THE_ONE, true, "mul-double/2addr");

    public static final Dop DIV_DOUBLE_2ADDR =
        new Dop(DalvOps.DIV_DOUBLE_2ADDR, DalvOps.DIV_DOUBLE,
            Form12x.THE_ONE, true, "div-double/2addr");

    public static final Dop REM_DOUBLE_2ADDR =
        new Dop(DalvOps.REM_DOUBLE_2ADDR, DalvOps.REM_DOUBLE,
            Form12x.THE_ONE, true, "rem-double/2addr");

    public static final Dop ADD_INT_LIT16 =
        new Dop(DalvOps.ADD_INT_LIT16, DalvOps.ADD_INT,
            Form22s.THE_ONE, true, "add-int/lit16");

    public static final Dop RSUB_INT =
        new Dop(DalvOps.RSUB_INT, DalvOps.RSUB_INT,
            Form22s.THE_ONE, true, "rsub-int");

    public static final Dop MUL_INT_LIT16 =
        new Dop(DalvOps.MUL_INT_LIT16, DalvOps.MUL_INT,
            Form22s.THE_ONE, true, "mul-int/lit16");

    public static final Dop DIV_INT_LIT16 =
        new Dop(DalvOps.DIV_INT_LIT16, DalvOps.DIV_INT,
            Form22s.THE_ONE, true, "div-int/lit16");

    public static final Dop REM_INT_LIT16 =
        new Dop(DalvOps.REM_INT_LIT16, DalvOps.REM_INT,
            Form22s.THE_ONE, true, "rem-int/lit16");

    public static final Dop AND_INT_LIT16 =
        new Dop(DalvOps.AND_INT_LIT16, DalvOps.AND_INT,
            Form22s.THE_ONE, true, "and-int/lit16");

    public static final Dop OR_INT_LIT16 =
        new Dop(DalvOps.OR_INT_LIT16, DalvOps.OR_INT,
            Form22s.THE_ONE, true, "or-int/lit16");

    public static final Dop XOR_INT_LIT16 =
        new Dop(DalvOps.XOR_INT_LIT16, DalvOps.XOR_INT,
            Form22s.THE_ONE, true, "xor-int/lit16");

    public static final Dop ADD_INT_LIT8 =
        new Dop(DalvOps.ADD_INT_LIT8, DalvOps.ADD_INT,
            Form22b.THE_ONE, true, "add-int/lit8");

    public static final Dop RSUB_INT_LIT8 =
        new Dop(DalvOps.RSUB_INT_LIT8, DalvOps.RSUB_INT,
            Form22b.THE_ONE, true, "rsub-int/lit8");

    public static final Dop MUL_INT_LIT8 =
        new Dop(DalvOps.MUL_INT_LIT8, DalvOps.MUL_INT,
            Form22b.THE_ONE, true, "mul-int/lit8");

    public static final Dop DIV_INT_LIT8 =
        new Dop(DalvOps.DIV_INT_LIT8, DalvOps.DIV_INT,
            Form22b.THE_ONE, true, "div-int/lit8");

    public static final Dop REM_INT_LIT8 =
        new Dop(DalvOps.REM_INT_LIT8, DalvOps.REM_INT,
            Form22b.THE_ONE, true, "rem-int/lit8");

    public static final Dop AND_INT_LIT8 =
        new Dop(DalvOps.AND_INT_LIT8, DalvOps.AND_INT,
            Form22b.THE_ONE, true, "and-int/lit8");

    public static final Dop OR_INT_LIT8 =
        new Dop(DalvOps.OR_INT_LIT8, DalvOps.OR_INT,
            Form22b.THE_ONE, true, "or-int/lit8");

    public static final Dop XOR_INT_LIT8 =
        new Dop(DalvOps.XOR_INT_LIT8, DalvOps.XOR_INT,
            Form22b.THE_ONE, true, "xor-int/lit8");

    public static final Dop SHL_INT_LIT8 =
        new Dop(DalvOps.SHL_INT_LIT8, DalvOps.SHL_INT,
            Form22b.THE_ONE, true, "shl-int/lit8");

    public static final Dop SHR_INT_LIT8 =
        new Dop(DalvOps.SHR_INT_LIT8, DalvOps.SHR_INT,
            Form22b.THE_ONE, true, "shr-int/lit8");

    public static final Dop USHR_INT_LIT8 =
        new Dop(DalvOps.USHR_INT_LIT8, DalvOps.USHR_INT,
            Form22b.THE_ONE, true, "ushr-int/lit8");

    // END(dops)

    // Static initialization.
    static {
        DOPS = new Dop[DalvOps.MAX_VALUE - DalvOps.MIN_VALUE + 1];

        set(SPECIAL_FORMAT);

        // BEGIN(dops-init); GENERATED AUTOMATICALLY BY opcode-gen
        set(NOP);
        set(MOVE);
        set(MOVE_FROM16);
        set(MOVE_16);
        set(MOVE_WIDE);
        set(MOVE_WIDE_FROM16);
        set(MOVE_WIDE_16);
        set(MOVE_OBJECT);
        set(MOVE_OBJECT_FROM16);
        set(MOVE_OBJECT_16);
        set(MOVE_RESULT);
        set(MOVE_RESULT_WIDE);
        set(MOVE_RESULT_OBJECT);
        set(MOVE_EXCEPTION);
        set(RETURN_VOID);
        set(RETURN);
        set(RETURN_WIDE);
        set(RETURN_OBJECT);
        set(CONST_4);
        set(CONST_16);
        set(CONST);
        set(CONST_HIGH16);
        set(CONST_WIDE_16);
        set(CONST_WIDE_32);
        set(CONST_WIDE);
        set(CONST_WIDE_HIGH16);
        set(CONST_STRING);
        set(CONST_STRING_JUMBO);
        set(CONST_CLASS);
        set(MONITOR_ENTER);
        set(MONITOR_EXIT);
        set(CHECK_CAST);
        set(INSTANCE_OF);
        set(ARRAY_LENGTH);
        set(NEW_INSTANCE);
        set(NEW_ARRAY);
        set(FILLED_NEW_ARRAY);
        set(FILLED_NEW_ARRAY_RANGE);
        set(FILL_ARRAY_DATA);
        set(THROW);
        set(GOTO);
        set(GOTO_16);
        set(GOTO_32);
        set(PACKED_SWITCH);
        set(SPARSE_SWITCH);
        set(CMPL_FLOAT);
        set(CMPG_FLOAT);
        set(CMPL_DOUBLE);
        set(CMPG_DOUBLE);
        set(CMP_LONG);
        set(IF_EQ);
        set(IF_NE);
        set(IF_LT);
        set(IF_GE);
        set(IF_GT);
        set(IF_LE);
        set(IF_EQZ);
        set(IF_NEZ);
        set(IF_LTZ);
        set(IF_GEZ);
        set(IF_GTZ);
        set(IF_LEZ);
        set(AGET);
        set(AGET_WIDE);
        set(AGET_OBJECT);
        set(AGET_BOOLEAN);
        set(AGET_BYTE);
        set(AGET_CHAR);
        set(AGET_SHORT);
        set(APUT);
        set(APUT_WIDE);
        set(APUT_OBJECT);
        set(APUT_BOOLEAN);
        set(APUT_BYTE);
        set(APUT_CHAR);
        set(APUT_SHORT);
        set(IGET);
        set(IGET_WIDE);
        set(IGET_OBJECT);
        set(IGET_BOOLEAN);
        set(IGET_BYTE);
        set(IGET_CHAR);
        set(IGET_SHORT);
        set(IPUT);
        set(IPUT_WIDE);
        set(IPUT_OBJECT);
        set(IPUT_BOOLEAN);
        set(IPUT_BYTE);
        set(IPUT_CHAR);
        set(IPUT_SHORT);
        set(SGET);
        set(SGET_WIDE);
        set(SGET_OBJECT);
        set(SGET_BOOLEAN);
        set(SGET_BYTE);
        set(SGET_CHAR);
        set(SGET_SHORT);
        set(SPUT);
        set(SPUT_WIDE);
        set(SPUT_OBJECT);
        set(SPUT_BOOLEAN);
        set(SPUT_BYTE);
        set(SPUT_CHAR);
        set(SPUT_SHORT);
        set(INVOKE_VIRTUAL);
        set(INVOKE_SUPER);
        set(INVOKE_DIRECT);
        set(INVOKE_STATIC);
        set(INVOKE_INTERFACE);
        set(INVOKE_VIRTUAL_RANGE);
        set(INVOKE_SUPER_RANGE);
        set(INVOKE_DIRECT_RANGE);
        set(INVOKE_STATIC_RANGE);
        set(INVOKE_INTERFACE_RANGE);
        set(NEG_INT);
        set(NOT_INT);
        set(NEG_LONG);
        set(NOT_LONG);
        set(NEG_FLOAT);
        set(NEG_DOUBLE);
        set(INT_TO_LONG);
        set(INT_TO_FLOAT);
        set(INT_TO_DOUBLE);
        set(LONG_TO_INT);
        set(LONG_TO_FLOAT);
        set(LONG_TO_DOUBLE);
        set(FLOAT_TO_INT);
        set(FLOAT_TO_LONG);
        set(FLOAT_TO_DOUBLE);
        set(DOUBLE_TO_INT);
        set(DOUBLE_TO_LONG);
        set(DOUBLE_TO_FLOAT);
        set(INT_TO_BYTE);
        set(INT_TO_CHAR);
        set(INT_TO_SHORT);
        set(ADD_INT);
        set(SUB_INT);
        set(MUL_INT);
        set(DIV_INT);
        set(REM_INT);
        set(AND_INT);
        set(OR_INT);
        set(XOR_INT);
        set(SHL_INT);
        set(SHR_INT);
        set(USHR_INT);
        set(ADD_LONG);
        set(SUB_LONG);
        set(MUL_LONG);
        set(DIV_LONG);
        set(REM_LONG);
        set(AND_LONG);
        set(OR_LONG);
        set(XOR_LONG);
        set(SHL_LONG);
        set(SHR_LONG);
        set(USHR_LONG);
        set(ADD_FLOAT);
        set(SUB_FLOAT);
        set(MUL_FLOAT);
        set(DIV_FLOAT);
        set(REM_FLOAT);
        set(ADD_DOUBLE);
        set(SUB_DOUBLE);
        set(MUL_DOUBLE);
        set(DIV_DOUBLE);
        set(REM_DOUBLE);
        set(ADD_INT_2ADDR);
        set(SUB_INT_2ADDR);
        set(MUL_INT_2ADDR);
        set(DIV_INT_2ADDR);
        set(REM_INT_2ADDR);
        set(AND_INT_2ADDR);
        set(OR_INT_2ADDR);
        set(XOR_INT_2ADDR);
        set(SHL_INT_2ADDR);
        set(SHR_INT_2ADDR);
        set(USHR_INT_2ADDR);
        set(ADD_LONG_2ADDR);
        set(SUB_LONG_2ADDR);
        set(MUL_LONG_2ADDR);
        set(DIV_LONG_2ADDR);
        set(REM_LONG_2ADDR);
        set(AND_LONG_2ADDR);
        set(OR_LONG_2ADDR);
        set(XOR_LONG_2ADDR);
        set(SHL_LONG_2ADDR);
        set(SHR_LONG_2ADDR);
        set(USHR_LONG_2ADDR);
        set(ADD_FLOAT_2ADDR);
        set(SUB_FLOAT_2ADDR);
        set(MUL_FLOAT_2ADDR);
        set(DIV_FLOAT_2ADDR);
        set(REM_FLOAT_2ADDR);
        set(ADD_DOUBLE_2ADDR);
        set(SUB_DOUBLE_2ADDR);
        set(MUL_DOUBLE_2ADDR);
        set(DIV_DOUBLE_2ADDR);
        set(REM_DOUBLE_2ADDR);
        set(ADD_INT_LIT16);
        set(RSUB_INT);
        set(MUL_INT_LIT16);
        set(DIV_INT_LIT16);
        set(REM_INT_LIT16);
        set(AND_INT_LIT16);
        set(OR_INT_LIT16);
        set(XOR_INT_LIT16);
        set(ADD_INT_LIT8);
        set(RSUB_INT_LIT8);
        set(MUL_INT_LIT8);
        set(DIV_INT_LIT8);
        set(REM_INT_LIT8);
        set(AND_INT_LIT8);
        set(OR_INT_LIT8);
        set(XOR_INT_LIT8);
        set(SHL_INT_LIT8);
        set(SHR_INT_LIT8);
        set(USHR_INT_LIT8);
        // END(dops-init)
    }

    /**
     * This class is uninstantiable.
     */
    private Dops() {
        // This space intentionally left blank.
    }

    /**
     * Gets the {@link Dop} for the given opcode value.
     *
     * @param opcode {@code DalvOps.MIN_VALUE..DalvOps.MAX_VALUE;} the opcode value
     * @return {@code non-null;} the associated opcode instance
     */
    public static Dop get(int opcode) {
        int idx = opcode - DalvOps.MIN_VALUE;

        try {
            Dop result = DOPS[idx];
            if (result != null) {
                return result;
            }
        } catch (ArrayIndexOutOfBoundsException ex) {
            // Fall through.
        }

        throw new IllegalArgumentException("bogus opcode");
    }

    /**
     * Gets the {@link Dop} with the given family/format combination, if
     * any.
     *
     * @param family {@code DalvOps.MIN_VALUE..DalvOps.MAX_VALUE;} the opcode family
     * @param format {@code non-null;} the opcode's instruction format
     * @return {@code null-ok;} the corresponding opcode, or {@code null} if
     * there is none
     */
    public static Dop getOrNull(int family, InsnFormat format) {
        if (format == null) {
            throw new NullPointerException("format == null");
        }

        int len = DOPS.length;

        // TODO: Linear search is bad.
        for (int i = 0; i < len; i++) {
            Dop dop = DOPS[i];
            if ((dop != null) &&
                (dop.getFamily() == family) &&
                (dop.getFormat() == format)) {
                return dop;
            }
        }

        return null;
    }

    /**
     * Puts the given opcode into the table of all ops.
     *
     * @param opcode {@code non-null;} the opcode
     */
    private static void set(Dop opcode) {
        int idx = opcode.getOpcode() - DalvOps.MIN_VALUE;
        DOPS[idx] = opcode;
    }
}
