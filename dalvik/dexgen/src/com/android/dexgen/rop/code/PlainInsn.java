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

package com.android.dexgen.rop.code;

import com.android.dexgen.rop.cst.Constant;
import com.android.dexgen.rop.type.StdTypeList;
import com.android.dexgen.rop.type.Type;
import com.android.dexgen.rop.type.TypeBearer;
import com.android.dexgen.rop.type.TypeList;

/**
 * Plain instruction, which has no embedded data and which cannot possibly
 * throw an exception.
 */
public final class PlainInsn
        extends Insn {
    /**
     * Constructs an instance.
     *
     * @param opcode {@code non-null;} the opcode
     * @param position {@code non-null;} source position
     * @param result {@code null-ok;} spec for the result, if any
     * @param sources {@code non-null;} specs for all the sources
     */
    public PlainInsn(Rop opcode, SourcePosition position,
                     RegisterSpec result, RegisterSpecList sources) {
        super(opcode, position, result, sources);

        switch (opcode.getBranchingness()) {
            case Rop.BRANCH_SWITCH:
            case Rop.BRANCH_THROW: {
                throw new IllegalArgumentException("bogus branchingness");
            }
        }

        if (result != null && opcode.getBranchingness() != Rop.BRANCH_NONE) {
            // move-result-pseudo is required here
            throw new IllegalArgumentException
                    ("can't mix branchingness with result");
        }
    }

    /**
     * Constructs a single-source instance.
     *
     * @param opcode {@code non-null;} the opcode
     * @param position {@code non-null;} source position
     * @param result {@code null-ok;} spec for the result, if any
     * @param source {@code non-null;} spec for the source
     */
    public PlainInsn(Rop opcode, SourcePosition position, RegisterSpec result,
                     RegisterSpec source) {
        this(opcode, position, result, RegisterSpecList.make(source));
    }

    /** {@inheritDoc} */
    @Override
    public TypeList getCatches() {
        return StdTypeList.EMPTY;
    }

    /** {@inheritDoc} */
    @Override
    public void accept(Visitor visitor) {
        visitor.visitPlainInsn(this);
    }

    /** {@inheritDoc} */
    @Override
    public Insn withAddedCatch(Type type) {
        throw new UnsupportedOperationException("unsupported");
    }

    /** {@inheritDoc} */
    @Override
    public Insn withRegisterOffset(int delta) {
        return new PlainInsn(getOpcode(), getPosition(),
                             getResult().withOffset(delta),
                             getSources().withOffset(delta));
    }

    /** {@inheritDoc} */
    @Override
    public Insn withLastSourceLiteral() {
        RegisterSpecList sources = getSources();
        int szSources = sources.size();

        if (szSources == 0) {
            return this;
        }

        TypeBearer lastType = sources.get(szSources - 1).getTypeBearer();

        if (!lastType.isConstant()) {
            return this;
        }

        Constant cst = (Constant) lastType;

        RegisterSpecList newSources = sources.withoutLast();

        Rop newRop;
        try {
            newRop = Rops.ropFor(getOpcode().getOpcode(),
                    getResult(), newSources, (Constant)lastType);
        } catch (IllegalArgumentException ex) {
            // There's no rop for this case
            return this;
        }

        return new PlainCstInsn(newRop, getPosition(),
                getResult(), newSources, cst);
    }


    /** {@inheritDoc} */
    @Override
    public Insn withNewRegisters(RegisterSpec result,
            RegisterSpecList sources) {

        return new PlainInsn(getOpcode(), getPosition(),
                             result,
                             sources);

    }
}
