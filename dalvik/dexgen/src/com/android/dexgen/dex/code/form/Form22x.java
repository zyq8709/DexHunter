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

import com.android.dexgen.dex.code.DalvInsn;
import com.android.dexgen.dex.code.InsnFormat;
import com.android.dexgen.dex.code.SimpleInsn;
import com.android.dexgen.rop.code.RegisterSpecList;
import com.android.dexgen.util.AnnotatedOutput;

/**
 * Instruction format {@code 22x}. See the instruction format spec
 * for details.
 */
public final class Form22x extends InsnFormat {
    /** {@code non-null;} unique instance of this class */
    public static final InsnFormat THE_ONE = new Form22x();

    /**
     * Constructs an instance. This class is not publicly
     * instantiable. Use {@link #THE_ONE}.
     */
    private Form22x() {
        // This space intentionally left blank.
    }

    /** {@inheritDoc} */
    @Override
    public String insnArgString(DalvInsn insn) {
        RegisterSpecList regs = insn.getRegisters();
        return regs.get(0).regString() + ", " + regs.get(1).regString();
    }

    /** {@inheritDoc} */
    @Override
    public String insnCommentString(DalvInsn insn, boolean noteIndices) {
        // This format has no comment.
        return "";
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

        return (insn instanceof SimpleInsn) &&
            (regs.size() == 2) &&
            unsignedFitsInByte(regs.get(0).getReg()) &&
            unsignedFitsInShort(regs.get(1).getReg());
    }

    /** {@inheritDoc} */
    @Override
    public InsnFormat nextUp() {
        return Form23x.THE_ONE;
    }

    /** {@inheritDoc} */
    @Override
    public void writeTo(AnnotatedOutput out, DalvInsn insn) {
        RegisterSpecList regs = insn.getRegisters();
        write(out,
              opcodeUnit(insn, regs.get(0).getReg()),
              (short) regs.get(1).getReg());
    }
}
