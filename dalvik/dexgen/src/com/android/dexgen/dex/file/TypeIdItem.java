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

package com.android.dexgen.dex.file;

import com.android.dexgen.rop.cst.CstType;
import com.android.dexgen.rop.cst.CstUtf8;
import com.android.dexgen.util.AnnotatedOutput;
import com.android.dexgen.util.Hex;

/**
 * Representation of a type reference inside a Dalvik file.
 */
public final class TypeIdItem extends IdItem {
    /** size of instances when written out to a file, in bytes */
    public static final int WRITE_SIZE = 4;

    /**
     * Constructs an instance.
     *
     * @param type {@code non-null;} the constant for the type
     */
    public TypeIdItem(CstType type) {
        super(type);
    }

    /** {@inheritDoc} */
    @Override
    public ItemType itemType() {
        return ItemType.TYPE_TYPE_ID_ITEM;
    }

    /** {@inheritDoc} */
    @Override
    public int writeSize() {
        return WRITE_SIZE;
    }

    /** {@inheritDoc} */
    @Override
    public void addContents(DexFile file) {
        file.getStringIds().intern(getDefiningClass().getDescriptor());
    }

    /** {@inheritDoc} */
    @Override
    public void writeTo(DexFile file, AnnotatedOutput out) {
        CstType type = getDefiningClass();
        CstUtf8 descriptor = type.getDescriptor();
        int idx = file.getStringIds().indexOf(descriptor);

        if (out.annotates()) {
            out.annotate(0, indexString() + ' ' + descriptor.toHuman());
            out.annotate(4, "  descriptor_idx: " + Hex.u4(idx));
        }

        out.writeInt(idx);
    }
}
