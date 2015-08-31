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

package com.android.dexgen.dex.code.form;

import com.android.dexgen.dex.code.CstInsn;
import com.android.dexgen.dex.code.DalvInsn;
import com.android.dexgen.dex.code.InsnFormat;
import com.android.dexgen.rop.code.RegisterSpecList;
import com.android.dexgen.rop.cst.Constant;
import com.android.dexgen.rop.cst.CstFieldRef;
import com.android.dexgen.rop.cst.CstString;
import com.android.dexgen.rop.cst.CstType;
import com.android.dexgen.util.AnnotatedOutput;

/**
 * Instruction format {@code 22c}. See the instruction format spec
 * for details.
 */
public final class Form22c extends InsnFormat {
    /** {@code non-null;} unique instance of this class */
    public static final InsnFormat THE_ONE = new Form22c();

    /**
     * Constructs an instance. This class is not publicly
     * instantiable. Use {@link #THE_ONE}.
     */
    private Form22c() {
        // This space intentionally left blank.
    }

    /** {@inheritDoc} */
    @Override
    public String insnArgString(DalvInsn insn) {
        RegisterSpecList regs = insn.getRegisters();
        return regs.get(0).regString() + ", " + regs.get(1).regString() +
            ", " + cstString(insn);
    }

    /** {@inheritDoc} */
    @Override
    public String insnCommentString(DalvInsn insn, boolean noteIndices) {
        if (noteIndices) {
            return cstComment(insn);
        } else {
            return "";
        }
    }

    /** {@inheritDoc} */
    @Override
    public int codeSize() {
        return 2;
    }

    /** {@inheritDoc} */
    @Override
    public boolean isCompatible(DalvInsn insn) {
        RegisterSpecList regs = insn.getRegisters();
        if (!((insn instanceof CstInsn) &&
              (regs.size() == 2) &&
              unsignedFitsInNibble(regs.get(0).getReg()) &&
              unsignedFitsInNibble(regs.get(1).getReg()))) {
            return false;
        }

        CstInsn ci = (CstInsn) insn;
        int cpi = ci.getIndex();

        if (! unsignedFitsInShort(cpi)) {
            return false;
        }

        Constant cst = ci.getConstant();
        return (cst instanceof CstType) ||
            (cst instanceof CstFieldRef);
    }

    /** {@inheritDoc} */
    @Override
    public InsnFormat nextUp() {
        return null;
    }

    /** {@inheritDoc} */
    @Override
    public void writeTo(AnnotatedOutput out, DalvInsn insn) {
        RegisterSpecList regs = insn.getRegisters();
        int cpi = ((CstInsn) insn).getIndex();

        write(out,
              opcodeUnit(insn,
                         makeByte(regs.get(0).getReg(), regs.get(1).getReg())),
              (short) cpi);
    }
}
