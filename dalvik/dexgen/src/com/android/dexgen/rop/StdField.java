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

package com.android.dexgen.rop;

import com.android.dexgen.rop.cst.CstNat;
import com.android.dexgen.rop.cst.CstType;
import com.android.dexgen.rop.cst.CstUtf8;
import com.android.dexgen.rop.cst.TypedConstant;

/**
 * Standard implementation of {@link Field}, which directly stores
 * all the associated data.
 */
public final class StdField extends StdMember implements Field {
    /**
     * Constructs an instance.
     *
     * @param definingClass {@code non-null;} the defining class
     * @param accessFlags access flags
     * @param nat {@code non-null;} member name and type (descriptor)
     * @param attributes {@code non-null;} list of associated attributes
     */
    public StdField(CstType definingClass, int accessFlags, CstNat nat,
                    AttributeList attributes) {
        super(definingClass, accessFlags, nat, attributes);
    }

    /**
     * Constructs an instance having Java field as its pattern.
     *
     * @param field {@code non-null;} pattern for dex field
     */
    public StdField(java.lang.reflect.Field field) {
        this(CstType.intern(field.getDeclaringClass()),
                field.getModifiers(),
                new CstNat(new CstUtf8(field.getName()),
                        CstType.intern(field.getType()).getDescriptor()),
                new StdAttributeList(0));
    }

    /**
     * Constructs an instance taking field description as user-friendly arguments.
     *
     * @param declaringClass {@code non-null;} the class field belongs to
     * @param type {@code non-null;} type of the field
     * @param name {@code non-null;} name of the field
     * @param modifiers access flags of the field
     */
    public StdField(Class definingClass, Class type, String name, int modifiers) {
        this(CstType.intern(definingClass),
                modifiers,
                new CstNat(new CstUtf8(name), CstType.intern(type).getDescriptor()),
                new StdAttributeList(0));
    }

    /** {@inheritDoc} */
    public TypedConstant getConstantValue() {
        AttributeList attribs = getAttributes();
        AttConstantValue cval = (AttConstantValue)
            attribs.findFirst(AttConstantValue.ATTRIBUTE_NAME);

        if (cval == null) {
            return null;
        }

        return cval.getConstantValue();
    }
}