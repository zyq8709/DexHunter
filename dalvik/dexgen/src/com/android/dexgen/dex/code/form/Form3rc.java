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
import com.android.dexgen.rop.code.RegisterSpec;
import com.android.dexgen.rop.code.RegisterSpecList;
import com.android.dexgen.rop.cst.Constant;
import com.android.dexgen.rop.cst.CstMethodRef;
import com.android.dexgen.rop.cst.CstType;
import com.android.dexgen.util.AnnotatedOutput;

/**
 * Instruction format {@code 3rc}. See the instruction format spec
 * for details.
 */
public final class Form3rc extends InsnFormat {
    /** {@code non-null;} unique instance of this class */
    public static final InsnFormat THE_ONE = new Form3rc();

    /**
     * Constructs an instance. This class is not publicly
     * instantiable. Use {@link #THE_ONE}.
     */
    private Form3rc() {
        // This space intentionally left blank.
    }

    /** {@inheritDoc} */
    @Override
    public String insnArgString(DalvInsn insn) {
        RegisterSpecList regs = insn.getRegisters();
        int size = regs.size();
        StringBuilder sb = new StringBuilder(30);

        sb.append("{");

        switch (size) {
            case 0: {
                // Nothing to do.
                break;
            }
            case 1: {
                sb.append(regs.get(0).regString());
                break;
            }
            default: {
                RegisterSpec lastReg = regs.get(size - 1);
                if (lastReg.getCategory() == 2) {
                    /*
                     * Add one to properly represent a list-final
                     * category-2 register.
                     */
                    lastReg = lastReg.withOffset(1);
                }

                sb.append(regs.get(0).regString());
                sb.append("..");
                sb.append(lastReg.regString());
            }
        }

        sb.append("}, ");
        sb.append(cstString(insn));

        return sb.toString();
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
        return 3;
    }

    /** {@inheritDoc} */
    @Override
    public boolean isCompatible(DalvInsn insn) {
        if (!(insn instanceof CstInsn)) {
            return false;
        }

        CstInsn ci = (CstInsn) insn;
        int cpi = ci.getIndex();

        if (! unsignedFitsInShort(cpi)) {
            return false;
        }

        Constant cst = ci.getConstant();
        if (!((cst instanceof CstMethodRef) ||
              (cst instanceof CstType))) {
            return false;
        }

        RegisterSpecList regs = ci.getRegisters();
        int sz = regs.size();

        if (sz == 0) {
            return true;
        }

        int first = regs.get(0).getReg();
        int next = first;

        if (!unsignedFitsInShort(first)) {
            return false;
        }

        for (int i = 0; i < sz; i++) {
            RegisterSpec one = regs.get(i);
            if (one.getReg() != next) {
                return false;
            }
            next += one.getCategory();
        }

        return unsignedFitsInByte(next - first);
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
        int sz = regs.size();
        int cpi = ((CstInsn) insn).getIndex();
        int firstReg;
        int count;

        if (sz == 0) {
            firstReg = 0;
            count = 0;
        } else {
            int lastReg = regs.get(sz - 1).getNextReg();
            firstReg = regs.get(0).getReg();
            count = lastReg - firstReg;
        }

        write(out,
              opcodeUnit(insn, count),
              (short) cpi,
              (short) firstReg);
    }
}
