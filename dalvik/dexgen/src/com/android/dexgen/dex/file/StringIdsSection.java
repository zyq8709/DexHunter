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

import com.android.dexgen.rop.cst.Constant;
import com.android.dexgen.rop.cst.CstNat;
import com.android.dexgen.rop.cst.CstString;
import com.android.dexgen.rop.cst.CstUtf8;
import com.android.dexgen.util.AnnotatedOutput;
import com.android.dexgen.util.Hex;

import java.util.Collection;
import java.util.TreeMap;

/**
 * Strings list section of a {@code .dex} file.
 */
public final class StringIdsSection
        extends UniformItemSection {
    /**
     * {@code non-null;} map from string constants to {@link
     * StringIdItem} instances
     */
    private final TreeMap<CstUtf8, StringIdItem> strings;

    /**
     * Constructs an instance. The file offset is initially unknown.
     *
     * @param file {@code non-null;} file that this instance is part of
     */
    public StringIdsSection(DexFile file) {
        super("string_ids", file, 4);

        strings = new TreeMap<CstUtf8, StringIdItem>();
    }

    /** {@inheritDoc} */
    @Override
    public Collection<? extends Item> items() {
        return strings.values();
    }

    /** {@inheritDoc} */
    @Override
    public IndexedItem get(Constant cst) {
        if (cst == null) {
            throw new NullPointerException("cst == null");
        }

        throwIfNotPrepared();

        if (cst instanceof CstString) {
            cst = ((CstString) cst).getString();
        }

        IndexedItem result = strings.get((CstUtf8) cst);

        if (result == null) {
            throw new IllegalArgumentException("not found");
        }

        return result;
    }

    /**
     * Writes the portion of the file header that refers to this instance.
     *
     * @param out {@code non-null;} where to write
     */
    public void writeHeaderPart(AnnotatedOutput out) {
        throwIfNotPrepared();

        int sz = strings.size();
        int offset = (sz == 0) ? 0 : getFileOffset();

        if (out.annotates()) {
            out.annotate(4, "string_ids_size: " + Hex.u4(sz));
            out.annotate(4, "string_ids_off:  " + Hex.u4(offset));
        }

        out.writeInt(sz);
        out.writeInt(offset);
    }

    /**
     * Interns an element into this instance.
     *
     * @param string {@code non-null;} the string to intern, as a regular Java
     * {@code String}
     * @return {@code non-null;} the interned string
     */
    public StringIdItem intern(String string) {
        CstUtf8 utf8 = new CstUtf8(string);
        return intern(new StringIdItem(utf8));
    }

    /**
     * Interns an element into this instance.
     *
     * @param string {@code non-null;} the string to intern, as a {@link CstString}
     * @return {@code non-null;} the interned string
     */
    public StringIdItem intern(CstString string) {
        CstUtf8 utf8 = string.getString();
        return intern(new StringIdItem(utf8));
    }

    /**
     * Interns an element into this instance.
     *
     * @param string {@code non-null;} the string to intern, as a constant
     * @return {@code non-null;} the interned string
     */
    public StringIdItem intern(CstUtf8 string) {
        return intern(new StringIdItem(string));
    }

    /**
     * Interns an element into this instance.
     *
     * @param string {@code non-null;} the string to intern
     * @return {@code non-null;} the interned string
     */
    public StringIdItem intern(StringIdItem string) {
        if (string == null) {
            throw new NullPointerException("string == null");
        }

        throwIfPrepared();

        CstUtf8 value = string.getValue();
        StringIdItem already = strings.get(value);

        if (already != null) {
            return already;
        }

        strings.put(value, string);
        return string;
    }

    /**
     * Interns the components of a name-and-type into this instance.
     *
     * @param nat {@code non-null;} the name-and-type
     */
    public void intern(CstNat nat) {
        intern(nat.getName());
        intern(nat.getDescriptor());
    }

    /**
     * Gets the index of the given string, which must have been added
     * to this instance.
     *
     * @param string {@code non-null;} the string to look up
     * @return {@code >= 0;} the string's index
     */
    public int indexOf(CstUtf8 string) {
        if (string == null) {
            throw new NullPointerException("string == null");
        }

        throwIfNotPrepared();

        StringIdItem s = strings.get(string);

        if (s == null) {
            throw new IllegalArgumentException("not found");
        }

        return s.getIndex();
    }

    /**
     * Gets the index of the given string, which must have been added
     * to this instance.
     *
     * @param string {@code non-null;} the string to look up
     * @return {@code >= 0;} the string's index
     */
    public int indexOf(CstString string) {
        return indexOf(string.getString());
    }

    /** {@inheritDoc} */
    @Override
    protected void orderItems() {
        int idx = 0;

        for (StringIdItem s : strings.values()) {
            s.setIndex(idx);
            idx++;
        }
    }
}
