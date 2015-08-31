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

package com.android.dexgen.rop.cst;

import com.android.dexgen.rop.type.Type;

/**
 * Constants of type {@code CONSTANT_String_info}.
 */
public final class CstString
        extends TypedConstant {
    /** {@code non-null;} the string value */
    private final CstUtf8 string;

    /**
     * Constructs an instance.
     *
     * @param string {@code non-null;} the string value
     */
    public CstString(CstUtf8 string) {
        if (string == null) {
            throw new NullPointerException("string == null");
        }

        this.string = string;
    }

    /**
     * Constructs an instance.
     *
     * @param string {@code non-null;} the string value
     */
    public CstString(String string) {
        this(new CstUtf8(string));
    }

    /** {@inheritDoc} */
    @Override
    public boolean equals(Object other) {
        if (!(other instanceof CstString)) {
            return false;
        }

        return string.equals(((CstString) other).string);
    }

    /** {@inheritDoc} */
    @Override
    public int hashCode() {
        return string.hashCode();
    }

    /** {@inheritDoc} */
    @Override
    protected int compareTo0(Constant other) {
        return string.compareTo(((CstString) other).string);
    }

    /** {@inheritDoc} */
    @Override
    public String toString() {
        return "string{" + toHuman() + '}';
    }

    /** {@inheritDoc} */
    public Type getType() {
        return Type.STRING;
    }

    /** {@inheritDoc} */
    @Override
    public String typeName() {
        return "string";
    }

    /** {@inheritDoc} */
    @Override
    public boolean isCategory2() {
        return false;
    }

    /** {@inheritDoc} */
    public String toHuman() {
        return string.toQuoted();
    }

    /**
     * Gets the string value.
     *
     * @return {@code non-null;} the string value
     */
    public CstUtf8 getString() {
        return string;
    }
}
