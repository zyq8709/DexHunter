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

/*
 * java.lang.reflect.Field
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"


/*
 * Validate access to a field.  Returns a pointer to the Field struct.
 *
 * "declaringClass" is the class in which the field was declared.  For an
 * instance field, "obj" is the object that holds the field data; for a
 * static field its value is ignored.
 *
 * "If the underlying field is static, the class that declared the
 * field is initialized if it has not already been initialized."
 *
 * On failure, throws an exception and returns NULL.
 *
 * The documentation lists exceptional conditions and the exceptions that
 * should be thrown, but doesn't say which exception prevails when two or
 * more exceptional conditions exist at the same time.  For example,
 * attempting to set a protected field from an unrelated class causes an
 * IllegalAccessException, while passing in a data type that doesn't match
 * the field causes an IllegalArgumentException.  If code does both at the
 * same time, we have to choose one or the other.
 *
 * The expected order is:
 *  (1) Check for illegal access. Throw IllegalAccessException.
 *  (2) Make sure the object actually has the field.  Throw
 *      IllegalArgumentException.
 *  (3) Make sure the field matches the expected type, e.g. if we issued
 *      a "getInteger" call make sure the field is an integer or can be
 *      converted to an int with a widening conversion.  Throw
 *      IllegalArgumentException.
 *  (4) Make sure "obj" is not null.  Throw NullPointerException.
 *
 * TODO: we're currently handling #3 after #4, because we don't check the
 * widening conversion until we're actually extracting the value from the
 * object (which won't work well if it's a null reference).
 */
static Field* validateFieldAccess(Object* obj, ClassObject* declaringClass,
    int slot, bool isSetOperation, bool noAccessCheck)
{
    Field* field;

    field = dvmSlotToField(declaringClass, slot);
    assert(field != NULL);

    /* verify access */
    if (!noAccessCheck) {
        if (isSetOperation && dvmIsFinalField(field)) {
            dvmThrowIllegalAccessException("field is marked 'final'");
            return NULL;
        }

        ClassObject* callerClass =
            dvmGetCaller2Class(dvmThreadSelf()->interpSave.curFrame);

        /*
         * We need to check two things:
         *  (1) Would an instance of the calling class have access to the field?
         *  (2) If the field is "protected", is the object an instance of the
         *      calling class, or is the field's declaring class in the same
         *      package as the calling class?
         *
         * #1 is basic access control.  #2 ensures that, just because
         * you're a subclass of Foo, you can't mess with protected fields
         * in arbitrary Foo objects from other packages.
         */
        if (!dvmCheckFieldAccess(callerClass, field)) {
            dvmThrowIllegalAccessException("access to field not allowed");
            return NULL;
        }
        if (dvmIsProtectedField(field)) {
            bool isInstance, samePackage;

            if (obj != NULL)
                isInstance = dvmInstanceof(obj->clazz, callerClass);
            else
                isInstance = false;
            samePackage = dvmInSamePackage(declaringClass, callerClass);

            if (!isInstance && !samePackage) {
                dvmThrowIllegalAccessException(
                    "access to protected field not allowed");
                return NULL;
            }
        }
    }

    if (dvmIsStaticField(field)) {
        /* init class if necessary, then return ptr to storage in "field" */
        if (!dvmIsClassInitialized(declaringClass)) {
            if (!dvmInitClass(declaringClass)) {
                assert(dvmCheckException(dvmThreadSelf()));
                return NULL;
            }
        }

    } else {
        /*
         * Verify object is of correct type (i.e. it actually has the
         * expected field in it), then grab a pointer to obj storage.
         * The call to dvmVerifyObjectInClass throws an NPE if "obj" is NULL.
         */
        if (!dvmVerifyObjectInClass(obj, declaringClass)) {
            assert(dvmCheckException(dvmThreadSelf()));
            return NULL;
        }
    }

    return field;
}

/*
 * Extracts the value of a static field.  Provides appropriate barriers
 * for volatile fields.
 *
 * Sub-32-bit values are sign- or zero-extended to fill out 32 bits.
 */
static void getStaticFieldValue(const StaticField* sfield, JValue* value)
{
    if (!dvmIsVolatileField(sfield)) {
        /* just copy the whole thing */
        *value = sfield->value;
    } else {
        /* need memory barriers and/or 64-bit atomic ops */
        switch (sfield->signature[0]) {
        case 'Z':
            value->i = dvmGetStaticFieldBooleanVolatile(sfield);
            break;
        case 'B':
            value->i = dvmGetStaticFieldByteVolatile(sfield);
            break;
        case 'S':
            value->i = dvmGetStaticFieldShortVolatile(sfield);
            break;
        case 'C':
            value->i = dvmGetStaticFieldCharVolatile(sfield);
            break;
        case 'I':
            value->i = dvmGetStaticFieldIntVolatile(sfield);
            break;
        case 'F':
            value->f = dvmGetStaticFieldFloatVolatile(sfield);
            break;
        case 'J':
            value->j = dvmGetStaticFieldLongVolatile(sfield);
            break;
        case 'D':
            value->d = dvmGetStaticFieldDoubleVolatile(sfield);
            break;
        case 'L':
        case '[':
            value->l = dvmGetStaticFieldObjectVolatile(sfield);
            break;
        default:
            ALOGE("Unhandled field signature '%s'", sfield->signature);
            dvmAbort();
        }
    }
}

/*
 * Extracts the value of an instance field.  Provides appropriate barriers
 * for volatile fields.
 *
 * Sub-32-bit values are sign- or zero-extended to fill out 32 bits.
 */
static void getInstFieldValue(const InstField* ifield, Object* obj,
    JValue* value)
{
    if (!dvmIsVolatileField(ifield)) {
        /* use type-specific get; really just 32-bit vs. 64-bit */
        switch (ifield->signature[0]) {
        case 'Z':
            value->i = dvmGetFieldBoolean(obj, ifield->byteOffset);
            break;
        case 'B':
            value->i = dvmGetFieldByte(obj, ifield->byteOffset);
            break;
        case 'S':
            value->i = dvmGetFieldShort(obj, ifield->byteOffset);
            break;
        case 'C':
            value->i = dvmGetFieldChar(obj, ifield->byteOffset);
            break;
        case 'I':
            value->i = dvmGetFieldInt(obj, ifield->byteOffset);
            break;
        case 'F':
            value->f = dvmGetFieldFloat(obj, ifield->byteOffset);
            break;
        case 'J':
            value->j = dvmGetFieldLong(obj, ifield->byteOffset);
            break;
        case 'D':
            value->d = dvmGetFieldDouble(obj, ifield->byteOffset);
            break;
        case 'L':
        case '[':
            value->l = dvmGetFieldObject(obj, ifield->byteOffset);
            break;
        default:
            ALOGE("Unhandled field signature '%s'", ifield->signature);
            dvmAbort();
        }
    } else {
        /* need memory barriers and/or 64-bit atomic ops */
        switch (ifield->signature[0]) {
        case 'Z':
            value->i = dvmGetFieldBooleanVolatile(obj, ifield->byteOffset);
            break;
        case 'B':
            value->i = dvmGetFieldByteVolatile(obj, ifield->byteOffset);
            break;
        case 'S':
            value->i = dvmGetFieldShortVolatile(obj, ifield->byteOffset);
            break;
        case 'C':
            value->i = dvmGetFieldCharVolatile(obj, ifield->byteOffset);
            break;
        case 'I':
            value->i = dvmGetFieldIntVolatile(obj, ifield->byteOffset);
            break;
        case 'F':
            value->f = dvmGetFieldFloatVolatile(obj, ifield->byteOffset);
            break;
        case 'J':
            value->j = dvmGetFieldLongVolatile(obj, ifield->byteOffset);
            break;
        case 'D':
            value->d = dvmGetFieldDoubleVolatile(obj, ifield->byteOffset);
            break;
        case 'L':
        case '[':
            value->l = dvmGetFieldObjectVolatile(obj, ifield->byteOffset);
            break;
        default:
            ALOGE("Unhandled field signature '%s'", ifield->signature);
            dvmAbort();
        }
    }
}

/*
 * Copies the value of the static or instance field into "*value".
 */
static void getFieldValue(const Field* field, Object* obj, JValue* value)
{
    if (dvmIsStaticField(field)) {
        return getStaticFieldValue((const StaticField*) field, value);
    } else {
        return getInstFieldValue((const InstField*) field, obj, value);
    }
}

/*
 * Sets the value of a static field.  Provides appropriate barriers
 * for volatile fields.
 */
static void setStaticFieldValue(StaticField* sfield, const JValue* value)
{
    if (!dvmIsVolatileField(sfield)) {
        switch (sfield->signature[0]) {
        case 'L':
        case '[':
            dvmSetStaticFieldObject(sfield, (Object*)value->l);
            break;
        default:
            /* just copy the whole thing */
            sfield->value = *value;
            break;
        }
    } else {
        /* need memory barriers and/or 64-bit atomic ops */
        switch (sfield->signature[0]) {
        case 'Z':
            dvmSetStaticFieldBooleanVolatile(sfield, value->z);
            break;
        case 'B':
            dvmSetStaticFieldByteVolatile(sfield, value->b);
            break;
        case 'S':
            dvmSetStaticFieldShortVolatile(sfield, value->s);
            break;
        case 'C':
            dvmSetStaticFieldCharVolatile(sfield, value->c);
            break;
        case 'I':
            dvmSetStaticFieldIntVolatile(sfield, value->i);
            break;
        case 'F':
            dvmSetStaticFieldFloatVolatile(sfield, value->f);
            break;
        case 'J':
            dvmSetStaticFieldLongVolatile(sfield, value->j);
            break;
        case 'D':
            dvmSetStaticFieldDoubleVolatile(sfield, value->d);
            break;
        case 'L':
        case '[':
            dvmSetStaticFieldObjectVolatile(sfield, (Object*)value->l);
            break;
        default:
            ALOGE("Unhandled field signature '%s'", sfield->signature);
            dvmAbort();
        }
    }
}

/*
 * Sets the value of an instance field.  Provides appropriate barriers
 * for volatile fields.
 */
static void setInstFieldValue(InstField* ifield, Object* obj,
    const JValue* value)
{
    if (!dvmIsVolatileField(ifield)) {
        /* use type-specific set; really just 32-bit vs. 64-bit */
        switch (ifield->signature[0]) {
        case 'Z':
            dvmSetFieldBoolean(obj, ifield->byteOffset, value->z);
            break;
        case 'B':
            dvmSetFieldByte(obj, ifield->byteOffset, value->b);
            break;
        case 'S':
            dvmSetFieldShort(obj, ifield->byteOffset, value->s);
            break;
        case 'C':
            dvmSetFieldChar(obj, ifield->byteOffset, value->c);
            break;
        case 'I':
            dvmSetFieldInt(obj, ifield->byteOffset, value->i);
            break;
        case 'F':
            dvmSetFieldFloat(obj, ifield->byteOffset, value->f);
            break;
        case 'J':
            dvmSetFieldLong(obj, ifield->byteOffset, value->j);
            break;
        case 'D':
            dvmSetFieldDouble(obj, ifield->byteOffset, value->d);
            break;
        case 'L':
        case '[':
            dvmSetFieldObject(obj, ifield->byteOffset, (Object *)value->l);
            break;
        default:
            ALOGE("Unhandled field signature '%s'", ifield->signature);
            dvmAbort();
        }
#if ANDROID_SMP != 0
        /*
         * Special handling for final fields on SMP systems.  We need a
         * store/store barrier here (JMM requirement).
         */
        if (dvmIsFinalField(ifield)) {
            ANDROID_MEMBAR_STORE();
        }
#endif
    } else {
        /* need memory barriers and/or 64-bit atomic ops */
        switch (ifield->signature[0]) {
        case 'Z':
            dvmSetFieldBooleanVolatile(obj, ifield->byteOffset, value->z);
            break;
        case 'B':
            dvmSetFieldByteVolatile(obj, ifield->byteOffset, value->b);
            break;
        case 'S':
            dvmSetFieldShortVolatile(obj, ifield->byteOffset, value->s);
            break;
        case 'C':
            dvmSetFieldCharVolatile(obj, ifield->byteOffset, value->c);
            break;
        case 'I':
            dvmSetFieldIntVolatile(obj, ifield->byteOffset, value->i);
            break;
        case 'F':
            dvmSetFieldFloatVolatile(obj, ifield->byteOffset, value->f);
            break;
        case 'J':
            dvmSetFieldLongVolatile(obj, ifield->byteOffset, value->j);
            break;
        case 'D':
            dvmSetFieldDoubleVolatile(obj, ifield->byteOffset, value->d);
            break;
        case 'L':
        case '[':
            dvmSetFieldObjectVolatile(obj, ifield->byteOffset, (Object*)value->l);
            break;
        default:
            ALOGE("Unhandled field signature '%s'", ifield->signature);
            dvmAbort();
        }
    }
}

/*
 * Copy "*value" into the static or instance field.
 */
static void setFieldValue(Field* field, Object* obj, const JValue* value)
{
    if (dvmIsStaticField(field)) {
        return setStaticFieldValue((StaticField*) field, value);
    } else {
        return setInstFieldValue((InstField*) field, obj, value);
    }
}



/*
 * public int getFieldModifiers(Class declaringClass, int slot)
 */
static void Dalvik_java_lang_reflect_Field_getFieldModifiers(const u4* args,
    JValue* pResult)
{
    /* ignore thisPtr in args[0] */
    ClassObject* declaringClass = (ClassObject*) args[1];
    int slot = args[2];
    Field* field;

    field = dvmSlotToField(declaringClass, slot);
    RETURN_INT(field->accessFlags & JAVA_FLAGS_MASK);
}

/*
 * private Object getField(Object o, Class declaringClass, Class type,
 *     int slot, boolean noAccessCheck)
 *
 * Primitive types need to be boxed.
 */
static void Dalvik_java_lang_reflect_Field_getField(const u4* args,
    JValue* pResult)
{
    /* ignore thisPtr in args[0] */
    Object* obj = (Object*) args[1];
    ClassObject* declaringClass = (ClassObject*) args[2];
    ClassObject* fieldType = (ClassObject*) args[3];
    int slot = args[4];
    bool noAccessCheck = (args[5] != 0);
    Field* field;
    JValue value;
    DataObject* result;

    //dvmDumpClass(obj->clazz, kDumpClassFullDetail);

    /* get a pointer to the Field after validating access */
    field = validateFieldAccess(obj, declaringClass, slot, false,noAccessCheck);
    if (field == NULL)
        RETURN_VOID();

    getFieldValue(field, obj, &value);

    /* if it's primitive, box it up */
    result = dvmBoxPrimitive(value, fieldType);
    dvmReleaseTrackedAlloc((Object*) result, NULL);
    RETURN_PTR(result);
}

/*
 * private void setField(Object o, Class declaringClass, Class type,
 *     int slot, boolean noAccessCheck, Object value)
 *
 * When assigning into a primitive field we will automatically extract
 * the value from box types.
 */
static void Dalvik_java_lang_reflect_Field_setField(const u4* args,
    JValue* pResult)
{
    /* ignore thisPtr in args[0] */
    Object* obj = (Object*) args[1];
    ClassObject* declaringClass = (ClassObject*) args[2];
    ClassObject* fieldType = (ClassObject*) args[3];
    int slot = args[4];
    bool noAccessCheck = (args[5] != 0);
    Object* valueObj = (Object*) args[6];
    Field* field;
    JValue value;

    /* unbox primitive, or verify object type */
    if (!dvmUnboxPrimitive(valueObj, fieldType, &value)) {
        dvmThrowIllegalArgumentException("invalid value for field");
        RETURN_VOID();
    }

    /* get a pointer to the Field after validating access */
    field = validateFieldAccess(obj, declaringClass, slot, true, noAccessCheck);

    if (field != NULL) {
        setFieldValue(field, obj, &value);
    }
    RETURN_VOID();
}

/*
 * Primitive field getters, e.g.:
 * private double getIField(Object o, Class declaringClass,
 *     Class type, int slot, boolean noAccessCheck, char descriptor)
 */
static void Dalvik_java_lang_reflect_Field_getPrimitiveField(const u4* args,
    JValue* pResult)
{
    /* ignore thisPtr in args[0] */
    Object* obj = (Object*) args[1];
    ClassObject* declaringClass = (ClassObject*) args[2];
    ClassObject* fieldType = (ClassObject*) args[3];
    int slot = args[4];
    bool noAccessCheck = (args[5] != 0);
    jchar descriptor = args[6];
    PrimitiveType targetType = dexGetPrimitiveTypeFromDescriptorChar(descriptor);
    const Field* field;
    JValue value;

    if (!dvmIsPrimitiveClass(fieldType)) {
        dvmThrowIllegalArgumentException("not a primitive field");
        RETURN_VOID();
    }

    /* get a pointer to the Field after validating access */
    field = validateFieldAccess(obj, declaringClass, slot, false,noAccessCheck);
    if (field == NULL)
        RETURN_VOID();

    getFieldValue(field, obj, &value);

    /* retrieve value, performing a widening conversion if necessary */
    if (dvmConvertPrimitiveValue(fieldType->primitiveType, targetType,
        &(value.i), &(pResult->i)) < 0)
    {
        dvmThrowIllegalArgumentException("invalid primitive conversion");
        RETURN_VOID();
    }
}

/*
 * Primitive field setters, e.g.:
 * private void setIField(Object o, Class declaringClass,
 *     Class type, int slot, boolean noAccessCheck, char descriptor, int value)
 */
static void Dalvik_java_lang_reflect_Field_setPrimitiveField(const u4* args,
    JValue* pResult)
{
    /* ignore thisPtr in args[0] */
    Object* obj = (Object*) args[1];
    ClassObject* declaringClass = (ClassObject*) args[2];
    ClassObject* fieldType = (ClassObject*) args[3];
    int slot = args[4];
    bool noAccessCheck = (args[5] != 0);
    jchar descriptor = args[6];
    const s4* valuePtr = (s4*) &args[7];    /* 64-bit vars spill into args[8] */
    PrimitiveType srcType = dexGetPrimitiveTypeFromDescriptorChar(descriptor);
    Field* field;
    JValue value;

    if (!dvmIsPrimitiveClass(fieldType)) {
        dvmThrowIllegalArgumentException("not a primitive field");
        RETURN_VOID();
    }

    /* convert the 32/64-bit arg to a JValue matching the field type */
    if (dvmConvertPrimitiveValue(srcType, fieldType->primitiveType,
        valuePtr, &(value.i)) < 0)
    {
        dvmThrowIllegalArgumentException("invalid primitive conversion");
        RETURN_VOID();
    }

    /* get a pointer to the Field after validating access */
    field = validateFieldAccess(obj, declaringClass, slot, true, noAccessCheck);

    if (field != NULL) {
        setFieldValue(field, obj, &value);
    }
    RETURN_VOID();
}

/*
 * private static Annotation[] getDeclaredAnnotations(
 *         Class declaringClass, int slot)
 *
 * Return the annotations declared for this field.
 */
static void Dalvik_java_lang_reflect_Field_getDeclaredAnnotations(
    const u4* args, JValue* pResult)
{
    ClassObject* declaringClass = (ClassObject*) args[0];
    int slot = args[1];
    Field* field;

    field = dvmSlotToField(declaringClass, slot);
    assert(field != NULL);

    ArrayObject* annos = dvmGetFieldAnnotations(field);
    dvmReleaseTrackedAlloc((Object*) annos, NULL);
    RETURN_PTR(annos);
}

/*
 * static Annotation getAnnotation(
 *         Class declaringClass, int slot, Class annotationType);
 */
static void Dalvik_java_lang_reflect_Field_getAnnotation(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    int slot = args[1];
    ClassObject* annotationClazz = (ClassObject*) args[2];

    Field* field = dvmSlotToField(clazz, slot);
    RETURN_PTR(dvmGetFieldAnnotation(clazz, field, annotationClazz));
}

/*
 * static boolean isAnnotationPresent(
 *         Class declaringClass, int slot, Class annotationType);
 */
static void Dalvik_java_lang_reflect_Field_isAnnotationPresent(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    int slot = args[1];
    ClassObject* annotationClazz = (ClassObject*) args[2];

    Field* field = dvmSlotToField(clazz, slot);
    RETURN_BOOLEAN(dvmIsFieldAnnotationPresent(clazz, field, annotationClazz));
}

/*
 * private Object[] getSignatureAnnotation()
 *
 * Returns the signature annotation.
 */
static void Dalvik_java_lang_reflect_Field_getSignatureAnnotation(const u4* args,
    JValue* pResult)
{
    /* ignore thisPtr in args[0] */
    ClassObject* declaringClass = (ClassObject*) args[1];
    int slot = args[2];
    Field* field;

    field = dvmSlotToField(declaringClass, slot);
    assert(field != NULL);

    ArrayObject* arr = dvmGetFieldSignatureAnnotation(field);
    dvmReleaseTrackedAlloc((Object*) arr, NULL);
    RETURN_PTR(arr);
}

const DalvikNativeMethod dvm_java_lang_reflect_Field[] = {
    { "getFieldModifiers",  "(Ljava/lang/Class;I)I",
        Dalvik_java_lang_reflect_Field_getFieldModifiers },
    { "getField",           "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZ)Ljava/lang/Object;",
        Dalvik_java_lang_reflect_Field_getField },
    { "getBField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZC)B",
        Dalvik_java_lang_reflect_Field_getPrimitiveField },
    { "getCField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZC)C",
        Dalvik_java_lang_reflect_Field_getPrimitiveField },
    { "getDField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZC)D",
        Dalvik_java_lang_reflect_Field_getPrimitiveField },
    { "getFField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZC)F",
        Dalvik_java_lang_reflect_Field_getPrimitiveField },
    { "getIField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZC)I",
        Dalvik_java_lang_reflect_Field_getPrimitiveField },
    { "getJField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZC)J",
        Dalvik_java_lang_reflect_Field_getPrimitiveField },
    { "getSField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZC)S",
        Dalvik_java_lang_reflect_Field_getPrimitiveField },
    { "getZField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZC)Z",
        Dalvik_java_lang_reflect_Field_getPrimitiveField },
    { "setField",           "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZLjava/lang/Object;)V",
        Dalvik_java_lang_reflect_Field_setField },
    { "setBField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZCB)V",
        Dalvik_java_lang_reflect_Field_setPrimitiveField },
    { "setCField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZCC)V",
        Dalvik_java_lang_reflect_Field_setPrimitiveField },
    { "setDField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZCD)V",
        Dalvik_java_lang_reflect_Field_setPrimitiveField },
    { "setFField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZCF)V",
        Dalvik_java_lang_reflect_Field_setPrimitiveField },
    { "setIField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZCI)V",
        Dalvik_java_lang_reflect_Field_setPrimitiveField },
    { "setJField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZCJ)V",
        Dalvik_java_lang_reflect_Field_setPrimitiveField },
    { "setSField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZCS)V",
        Dalvik_java_lang_reflect_Field_setPrimitiveField },
    { "setZField",          "(Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/Class;IZCZ)V",
        Dalvik_java_lang_reflect_Field_setPrimitiveField },
    { "getDeclaredAnnotations", "(Ljava/lang/Class;I)[Ljava/lang/annotation/Annotation;",
        Dalvik_java_lang_reflect_Field_getDeclaredAnnotations },
    { "getAnnotation", "(Ljava/lang/Class;ILjava/lang/Class;)Ljava/lang/annotation/Annotation;",
        Dalvik_java_lang_reflect_Field_getAnnotation },
    { "isAnnotationPresent", "(Ljava/lang/Class;ILjava/lang/Class;)Z",
        Dalvik_java_lang_reflect_Field_isAnnotationPresent },
    { "getSignatureAnnotation",  "(Ljava/lang/Class;I)[Ljava/lang/Object;",
        Dalvik_java_lang_reflect_Field_getSignatureAnnotation },
    { NULL, NULL, NULL },
};
