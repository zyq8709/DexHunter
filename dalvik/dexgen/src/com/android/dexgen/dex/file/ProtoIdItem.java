/*
 * Copyright (C) 2008 The Android Open Source Project
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

package com.android.dexgen.dex.file;

import com.android.dexgen.rop.cst.CstType;
import com.android.dexgen.rop.cst.CstUtf8;
import com.android.dexgen.rop.type.Prototype;
import com.android.dexgen.rop.type.StdTypeList;
import com.android.dexgen.rop.type.Type;
import com.android.dexgen.util.AnnotatedOutput;
import com.android.dexgen.util.Hex;

/**
 * Representation of a method prototype reference inside a Dalvik file.
 */
public final class ProtoIdItem extends IndexedItem {
    /** size of instances when written out to a file, in bytes */
    public static final int WRITE_SIZE = 12;

    /** {@code non-null;} the wrapped prototype */
    private final Prototype prototype;

    /** {@code non-null;} the short-form of the prototype */
    private final CstUtf8 shortForm;

    /**
     * {@code null-ok;} the list of parameter types or {@code null} if this
     * prototype has no parameters
     */
    private TypeListItem parameterTypes;

    /**
     * Constructs an instance.
     *
     * @param prototype {@code non-null;} the constant for the prototype
     */
    public ProtoIdItem(Prototype prototype) {
        if (prototype == null) {
            throw new NullPointerException("prototype == null");
        }

        this.prototype = prototype;
        this.shortForm = makeShortForm(prototype);

        StdTypeList parameters = prototype.getParameterTypes();
        this.parameterTypes = (parameters.size() == 0) ? null
            : new TypeListItem(parameters);
    }

    /**
     * Creates the short-form of the given prototype.
     *
     * @param prototype {@code non-null;} the prototype
     * @return {@code non-null;} the short form
     */
    private static CstUtf8 makeShortForm(Prototype prototype) {
        StdTypeList parameters = prototype.getParameterTypes();
        int size = parameters.size();
        StringBuilder sb = new StringBuilder(size + 1);

        sb.append(shortFormCharFor(prototype.getReturnType()));

        for (int i = 0; i < size; i++) {
            sb.append(shortFormCharFor(parameters.getType(i)));
        }

        return new CstUtf8(sb.toString());
    }

    /**
     * Gets the short-form character for the given type.
     *
     * @param type {@code non-null;} the type
     * @return the corresponding short-form character
     */
    private static char shortFormCharFor(Type type) {
        char descriptorChar = type.getDescriptor().charAt(0);

        if (descriptorChar == '[') {
            return 'L';
        }

        return descriptorChar;
    }

    /** {@inheritDoc} */
    @Override
    public ItemType itemType() {
        return ItemType.TYPE_PROTO_ID_ITEM;
    }

    /** {@inheritDoc} */
    @Override
    public int writeSize() {
        return WRITE_SIZE;
    }

    /** {@inheritDoc} */
    @Override
    public void addContents(DexFile file) {
        StringIdsSection stringIds = file.getStringIds();
        TypeIdsSection typeIds = file.getTypeIds();
        MixedItemSection typeLists = file.getTypeLists();

        typeIds.intern(prototype.getReturnType());
        stringIds.intern(shortForm);

        if (parameterTypes != null) {
            parameterTypes = typeLists.intern(parameterTypes);
        }
    }

    /** {@inheritDoc} */
    @Override
    public void writeTo(DexFile file, AnnotatedOutput out) {
        int shortyIdx = file.getStringIds().indexOf(shortForm);
        int returnIdx = file.getTypeIds().indexOf(prototype.getReturnType());
        int paramsOff = OffsettedItem.getAbsoluteOffsetOr0(parameterTypes);

        if (out.annotates()) {
            StringBuilder sb = new StringBuilder();
            sb.append(prototype.getReturnType().toHuman());
            sb.append(" proto(");

            StdTypeList params = prototype.getParameterTypes();
            int size = params.size();

            for (int i = 0; i < size; i++) {
                if (i != 0) {
                    sb.append(", ");
                }
                sb.append(params.getType(i).toHuman());
            }

            sb.append(")");
            out.annotate(0, indexString() + ' ' + sb.toString());
            out.annotate(4, "  shorty_idx:      " + Hex.u4(shortyIdx) +
                    " // " + shortForm.toQuoted());
            out.annotate(4, "  return_type_idx: " + Hex.u4(returnIdx) +
                    " // " + prototype.getReturnType().toHuman());
            out.annotate(4, "  parameters_off:  " + Hex.u4(paramsOff));
        }

        out.writeInt(shortyIdx);
        out.writeInt(returnIdx);
        out.writeInt(paramsOff);
    }
}
