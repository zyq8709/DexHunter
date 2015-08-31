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
 * Annotations.
 *
 * We're not expecting to make much use of runtime annotations, so speed vs.
 * space choices are weighted heavily toward small size.
 *
 * It would have been nice to treat "system" annotations in the same way
 * we do "real" annotations, but that doesn't work.  The chief difficulty
 * is that some of them have member types that are not legal in annotations,
 * such as Method and Annotation.  Another source of pain comes from the
 * AnnotationDefault annotation, which by virtue of being an annotation
 * could itself have default values, requiring some additional checks to
 * prevent recursion.
 *
 * It's simpler, and more efficient, to handle the system annotations
 * entirely inside the VM.  There are empty classes defined for the system
 * annotation types, but their only purpose is to allow the system
 * annotations to share name space with standard annotations.
 */
#include "Dalvik.h"

// fwd
static Object* processEncodedAnnotation(const ClassObject* clazz,\
    const u1** pPtr);
static bool skipEncodedAnnotation(const ClassObject* clazz, const u1** pPtr);

/*
 * System annotation descriptors.
 */
static const char* kDescrAnnotationDefault
                                    = "Ldalvik/annotation/AnnotationDefault;";
static const char* kDescrEnclosingClass
                                    = "Ldalvik/annotation/EnclosingClass;";
static const char* kDescrEnclosingMethod
                                    = "Ldalvik/annotation/EnclosingMethod;";
static const char* kDescrInnerClass = "Ldalvik/annotation/InnerClass;";
static const char* kDescrMemberClasses
                                    = "Ldalvik/annotation/MemberClasses;";
static const char* kDescrSignature  = "Ldalvik/annotation/Signature;";
static const char* kDescrThrows     = "Ldalvik/annotation/Throws;";

/*
 * Read an unsigned LEB128 value from a buffer.  Advances "pBuf".
 */
static u4 readUleb128(const u1** pBuf)
{
    u4 result = 0;
    int shift = 0;
    const u1* buf = *pBuf;
    u1 val;

    do {
        /*
         * Worst-case on bad data is we read too much data and return a bogus
         * result.  Safe to assume that we will encounter a byte with its
         * high bit clear before the end of the mapped file.
         */
        assert(shift < 32);

        val = *buf++;
        result |= (val & 0x7f) << shift;
        shift += 7;
    } while ((val & 0x80) != 0);

    *pBuf = buf;
    return result;
}

/*
 * Get the annotations directory item.
 */
static const DexAnnotationsDirectoryItem* getAnnoDirectory(DexFile* pDexFile,
    const ClassObject* clazz)
{
    const DexClassDef* pClassDef;

    /*
     * Find the class def in the DEX file.  For better performance we should
     * stash this in the ClassObject.
     */
    pClassDef = dexFindClass(pDexFile, clazz->descriptor);
    assert(pClassDef != NULL);
    return dexGetAnnotationsDirectoryItem(pDexFile, pClassDef);
}

/*
 * Return a zero-length array of Annotation objects.
 *
 * TODO: this currently allocates a new array each time, but I think we
 * can get away with returning a canonical copy.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
static ArrayObject* emptyAnnoArray()
{
    return dvmAllocArrayByClass(
        gDvm.classJavaLangAnnotationAnnotationArray, 0, ALLOC_DEFAULT);
}

/*
 * Return an array of empty arrays of Annotation objects.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
static ArrayObject* emptyAnnoArrayArray(int numElements)
{
    Thread* self = dvmThreadSelf();
    ArrayObject* arr;
    int i;

    arr = dvmAllocArrayByClass(gDvm.classJavaLangAnnotationAnnotationArrayArray,
            numElements, ALLOC_DEFAULT);
    if (arr != NULL) {
        ArrayObject** elems = (ArrayObject**)(void*)arr->contents;
        for (i = 0; i < numElements; i++) {
            elems[i] = emptyAnnoArray();
            dvmReleaseTrackedAlloc((Object*)elems[i], self);
        }
    }

    return arr;
}

/*
 * Read a signed integer.  "zwidth" is the zero-based byte count.
 */
static s4 readSignedInt(const u1* ptr, int zwidth)
{
    s4 val = 0;
    int i;

    for (i = zwidth; i >= 0; --i)
        val = ((u4)val >> 8) | (((s4)*ptr++) << 24);
    val >>= (3 - zwidth) * 8;

    return val;
}

/*
 * Read an unsigned integer.  "zwidth" is the zero-based byte count,
 * "fillOnRight" indicates which side we want to zero-fill from.
 */
static u4 readUnsignedInt(const u1* ptr, int zwidth, bool fillOnRight)
{
    u4 val = 0;
    int i;

    if (!fillOnRight) {
        for (i = zwidth; i >= 0; --i)
            val = (val >> 8) | (((u4)*ptr++) << 24);
        val >>= (3 - zwidth) * 8;
    } else {
        for (i = zwidth; i >= 0; --i)
            val = (val >> 8) | (((u4)*ptr++) << 24);
    }
    return val;
}

/*
 * Read a signed long.  "zwidth" is the zero-based byte count.
 */
static s8 readSignedLong(const u1* ptr, int zwidth)
{
    s8 val = 0;
    int i;

    for (i = zwidth; i >= 0; --i)
        val = ((u8)val >> 8) | (((s8)*ptr++) << 56);
    val >>= (7 - zwidth) * 8;

    return val;
}

/*
 * Read an unsigned long.  "zwidth" is the zero-based byte count,
 * "fillOnRight" indicates which side we want to zero-fill from.
 */
static u8 readUnsignedLong(const u1* ptr, int zwidth, bool fillOnRight)
{
    u8 val = 0;
    int i;

    if (!fillOnRight) {
        for (i = zwidth; i >= 0; --i)
            val = (val >> 8) | (((u8)*ptr++) << 56);
        val >>= (7 - zwidth) * 8;
    } else {
        for (i = zwidth; i >= 0; --i)
            val = (val >> 8) | (((u8)*ptr++) << 56);
    }
    return val;
}


/*
 * ===========================================================================
 *      Element extraction
 * ===========================================================================
 */

/*
 * An annotation in "clazz" refers to a method by index.  This just gives
 * us the name of the class and the name and signature of the method.  We
 * need to find the method's class, and then find the method within that
 * class.  If the method has been resolved before, we can just use the
 * results of the previous lookup.
 *
 * Normally we do this as part of method invocation in the interpreter, which
 * provides us with a bit of context: is it virtual or direct, do we need
 * to initialize the class because it's a static method, etc.  We don't have
 * that information here, so we have to do a bit of searching.
 *
 * Returns NULL if the method was not found (exception may be pending).
 */
static Method* resolveAmbiguousMethod(const ClassObject* referrer, u4 methodIdx)
{
    DexFile* pDexFile;
    ClassObject* resClass;
    Method* resMethod;
    const DexMethodId* pMethodId;
    const char* name;

    /* if we've already resolved this method, return it */
    resMethod = dvmDexGetResolvedMethod(referrer->pDvmDex, methodIdx);
    if (resMethod != NULL)
        return resMethod;

    pDexFile = referrer->pDvmDex->pDexFile;
    pMethodId = dexGetMethodId(pDexFile, methodIdx);
    resClass = dvmResolveClass(referrer, pMethodId->classIdx, true);
    if (resClass == NULL) {
        /* note exception will be pending */
        ALOGD("resolveAmbiguousMethod: unable to find class %d", methodIdx);
        return NULL;
    }
    if (dvmIsInterfaceClass(resClass)) {
        /* method is part of an interface -- not expecting that */
        ALOGD("resolveAmbiguousMethod: method in interface?");
        return NULL;
    }

    // TODO - consider a method access flag that indicates direct vs. virtual
    name = dexStringById(pDexFile, pMethodId->nameIdx);

    DexProto proto;
    dexProtoSetFromMethodId(&proto, pDexFile, pMethodId);

    if (name[0] == '<') {
        /*
         * Constructor or class initializer.  Only need to examine the
         * "direct" list, and don't need to look up the class hierarchy.
         */
        resMethod = dvmFindDirectMethod(resClass, name, &proto);
    } else {
        /*
         * Do a hierarchical scan for direct and virtual methods.
         *
         * This uses the search order from the VM spec (v2 5.4.3.3), which
         * seems appropriate here.
         */
        resMethod = dvmFindMethodHier(resClass, name, &proto);
    }

    return resMethod;
}

/*
 * constants for processAnnotationValue indicating what style of
 * result is wanted
 */
enum AnnotationResultStyle {
    kAllObjects,         /* return everything as an object */
    kAllRaw,             /* return everything as a raw value or index */
    kPrimitivesOrObjects /* return primitives as-is but the rest as objects */
};

/*
 * Recursively process an annotation value.
 *
 * "clazz" is the class on which the annotations are defined.  It may be
 * NULL when "resultStyle" is "kAllRaw".
 *
 * If "resultStyle" is "kAllObjects", the result will always be an Object of an
 * appropriate type (in pValue->value.l).  For primitive types, the usual
 * wrapper objects will be created.
 *
 * If "resultStyle" is "kAllRaw", numeric constants are stored directly into
 * "pValue", and indexed values like String and Method are returned as
 * indexes.  Complex values like annotations and arrays are not handled.
 *
 * If "resultStyle" is "kPrimitivesOrObjects", numeric constants are stored
 * directly into "pValue", and everything else is constructed as an Object
 * of appropriate type (in pValue->value.l).
 *
 * The caller must call dvmReleaseTrackedAlloc on returned objects, when
 * using "kAllObjects" or "kPrimitivesOrObjects".
 *
 * Returns "true" on success, "false" if the value could not be processed
 * or an object could not be allocated.  On allocation failure an exception
 * will be raised.
 */
static bool processAnnotationValue(const ClassObject* clazz,
    const u1** pPtr, AnnotationValue* pValue,
    AnnotationResultStyle resultStyle)
{
    Thread* self = dvmThreadSelf();
    Object* elemObj = NULL;
    bool setObject = false;
    const u1* ptr = *pPtr;
    u1 valueType, valueArg;
    int width;
    u4 idx;

    valueType = *ptr++;
    valueArg = valueType >> kDexAnnotationValueArgShift;
    width = valueArg + 1;       /* assume, correct later */

    ALOGV("----- type is 0x%02x %d, ptr=%p [0x%06x]",
        valueType & kDexAnnotationValueTypeMask, valueArg, ptr-1,
        (ptr-1) - (u1*)clazz->pDvmDex->pDexFile->baseAddr);

    pValue->type = valueType & kDexAnnotationValueTypeMask;

    switch (valueType & kDexAnnotationValueTypeMask) {
    case kDexAnnotationByte:
        pValue->value.i = (s1) readSignedInt(ptr, valueArg);
        if (resultStyle == kAllObjects) {
            elemObj = (Object*) dvmBoxPrimitive(pValue->value,
                        dvmFindPrimitiveClass('B'));
            setObject = true;
        }
        break;
    case kDexAnnotationShort:
        pValue->value.i = (s2) readSignedInt(ptr, valueArg);
        if (resultStyle == kAllObjects) {
            elemObj = (Object*) dvmBoxPrimitive(pValue->value,
                        dvmFindPrimitiveClass('S'));
            setObject = true;
        }
        break;
    case kDexAnnotationChar:
        pValue->value.i = (u2) readUnsignedInt(ptr, valueArg, false);
        if (resultStyle == kAllObjects) {
            elemObj = (Object*) dvmBoxPrimitive(pValue->value,
                        dvmFindPrimitiveClass('C'));
            setObject = true;
        }
        break;
    case kDexAnnotationInt:
        pValue->value.i = readSignedInt(ptr, valueArg);
        if (resultStyle == kAllObjects) {
            elemObj = (Object*) dvmBoxPrimitive(pValue->value,
                        dvmFindPrimitiveClass('I'));
            setObject = true;
        }
        break;
    case kDexAnnotationLong:
        pValue->value.j = readSignedLong(ptr, valueArg);
        if (resultStyle == kAllObjects) {
            elemObj = (Object*) dvmBoxPrimitive(pValue->value,
                        dvmFindPrimitiveClass('J'));
            setObject = true;
        }
        break;
    case kDexAnnotationFloat:
        pValue->value.i = readUnsignedInt(ptr, valueArg, true);
        if (resultStyle == kAllObjects) {
            elemObj = (Object*) dvmBoxPrimitive(pValue->value,
                        dvmFindPrimitiveClass('F'));
            setObject = true;
        }
        break;
    case kDexAnnotationDouble:
        pValue->value.j = readUnsignedLong(ptr, valueArg, true);
        if (resultStyle == kAllObjects) {
            elemObj = (Object*) dvmBoxPrimitive(pValue->value,
                        dvmFindPrimitiveClass('D'));
            setObject = true;
        }
        break;
    case kDexAnnotationBoolean:
        pValue->value.i = (valueArg != 0);
        if (resultStyle == kAllObjects) {
            elemObj = (Object*) dvmBoxPrimitive(pValue->value,
                        dvmFindPrimitiveClass('Z'));
            setObject = true;
        }
        width = 0;
        break;

    case kDexAnnotationString:
        idx = readUnsignedInt(ptr, valueArg, false);
        if (resultStyle == kAllRaw) {
            pValue->value.i = idx;
        } else {
            elemObj = (Object*) dvmResolveString(clazz, idx);
            setObject = true;
            if (elemObj == NULL)
                return false;
            dvmAddTrackedAlloc(elemObj, self);      // balance the Release
        }
        break;
    case kDexAnnotationType:
        idx = readUnsignedInt(ptr, valueArg, false);
        if (resultStyle == kAllRaw) {
            pValue->value.i = idx;
        } else {
            elemObj = (Object*) dvmResolveClass(clazz, idx, true);
            setObject = true;
            if (elemObj == NULL) {
                /* we're expected to throw a TypeNotPresentException here */
                DexFile* pDexFile = clazz->pDvmDex->pDexFile;
                const char* desc = dexStringByTypeIdx(pDexFile, idx);
                dvmClearException(self);
                dvmThrowTypeNotPresentException(desc);
                return false;
            } else {
                dvmAddTrackedAlloc(elemObj, self);      // balance the Release
            }
        }
        break;
    case kDexAnnotationMethod:
        idx = readUnsignedInt(ptr, valueArg, false);
        if (resultStyle == kAllRaw) {
            pValue->value.i = idx;
        } else {
            Method* meth = resolveAmbiguousMethod(clazz, idx);
            if (meth == NULL)
                return false;
            elemObj = dvmCreateReflectObjForMethod(clazz, meth);
            setObject = true;
            if (elemObj == NULL)
                return false;
        }
        break;
    case kDexAnnotationField:
        idx = readUnsignedInt(ptr, valueArg, false);
        assert(false);      // TODO
        break;
    case kDexAnnotationEnum:
        /* enum values are the contents of a static field */
        idx = readUnsignedInt(ptr, valueArg, false);
        if (resultStyle == kAllRaw) {
            pValue->value.i = idx;
        } else {
            StaticField* sfield;

            sfield = dvmResolveStaticField(clazz, idx);
            if (sfield == NULL) {
                return false;
            } else {
                assert(sfield->clazz->descriptor[0] == 'L');
                elemObj = sfield->value.l;
                setObject = true;
                dvmAddTrackedAlloc(elemObj, self);      // balance the Release
            }
        }
        break;
    case kDexAnnotationArray:
        /*
         * encoded_array format, which is a size followed by a stream
         * of annotation_value.
         *
         * We create an array of Object, populate it, and return it.
         */
        if (resultStyle == kAllRaw) {
            return false;
        } else {
            ArrayObject* newArray;
            u4 size, count;

            size = readUleb128(&ptr);
            LOGVV("--- annotation array, size is %u at %p", size, ptr);
            newArray = dvmAllocArrayByClass(gDvm.classJavaLangObjectArray,
                size, ALLOC_DEFAULT);
            if (newArray == NULL) {
                ALOGE("annotation element array alloc failed (%d)", size);
                return false;
            }

            AnnotationValue avalue;
            for (count = 0; count < size; count++) {
                if (!processAnnotationValue(clazz, &ptr, &avalue,
                                kAllObjects)) {
                    dvmReleaseTrackedAlloc((Object*)newArray, self);
                    return false;
                }
                Object* obj = (Object*)avalue.value.l;
                dvmSetObjectArrayElement(newArray, count, obj);
                dvmReleaseTrackedAlloc(obj, self);
            }

            elemObj = (Object*) newArray;
            setObject = true;
        }
        width = 0;
        break;
    case kDexAnnotationAnnotation:
        /* encoded_annotation format */
        if (resultStyle == kAllRaw)
            return false;
        elemObj = processEncodedAnnotation(clazz, &ptr);
        setObject = true;
        if (elemObj == NULL)
            return false;
        dvmAddTrackedAlloc(elemObj, self);      // balance the Release
        width = 0;
        break;
    case kDexAnnotationNull:
        if (resultStyle == kAllRaw) {
            pValue->value.i = 0;
        } else {
            assert(elemObj == NULL);
            setObject = true;
        }
        width = 0;
        break;
    default:
        ALOGE("Bad annotation element value byte 0x%02x (0x%02x)",
            valueType, valueType & kDexAnnotationValueTypeMask);
        assert(false);
        return false;
    }

    ptr += width;

    *pPtr = ptr;
    if (setObject)
        pValue->value.l = elemObj;
    return true;
}


/*
 * For most object types, we have nothing to do here, and we just return
 * "valueObj".
 *
 * For an array annotation, the type of the extracted object will always
 * be java.lang.Object[], but we want it to match the type that the
 * annotation member is expected to return.  In some cases this may
 * involve un-boxing primitive values.
 *
 * We allocate a second array with the correct type, then copy the data
 * over.  This releases the tracked allocation on "valueObj" and returns
 * a new, tracked object.
 *
 * On failure, this releases the tracking on "valueObj" and returns NULL
 * (allowing the call to say "foo = convertReturnType(foo, ..)").
 */
static Object* convertReturnType(Object* valueObj, ClassObject* methodReturn)
{
    if (valueObj == NULL ||
        !dvmIsArray((ArrayObject*)valueObj) || !dvmIsArrayClass(methodReturn))
    {
        return valueObj;
    }

    Thread* self = dvmThreadSelf();
    ClassObject* srcElemClass;
    ClassObject* dstElemClass;

    /*
     * We always extract kDexAnnotationArray into Object[], so we expect to
     * find that here.  This means we can skip the FindClass on
     * (valueObj->clazz->descriptor+1, valueObj->clazz->classLoader).
     */
    if (strcmp(valueObj->clazz->descriptor, "[Ljava/lang/Object;") != 0) {
        ALOGE("Unexpected src type class (%s)", valueObj->clazz->descriptor);
        return NULL;
    }
    srcElemClass = gDvm.classJavaLangObject;

    /*
     * Skip past the '[' to get element class name.  Note this is not always
     * the same as methodReturn->elementClass.
     */
    char firstChar = methodReturn->descriptor[1];
    if (firstChar == 'L' || firstChar == '[') {
        dstElemClass = dvmFindClass(methodReturn->descriptor+1,
            methodReturn->classLoader);
    } else {
        dstElemClass = dvmFindPrimitiveClass(firstChar);
    }
    ALOGV("HEY: converting valueObj from [%s to [%s",
        srcElemClass->descriptor, dstElemClass->descriptor);

    ArrayObject* srcArray = (ArrayObject*) valueObj;
    u4 length = srcArray->length;
    ArrayObject* newArray;

    newArray = dvmAllocArrayByClass(methodReturn, length, ALLOC_DEFAULT);
    if (newArray == NULL) {
        ALOGE("Failed creating duplicate annotation class (%s %d)",
            methodReturn->descriptor, length);
        goto bail;
    }

    bool success;
    if (dstElemClass->primitiveType == PRIM_NOT) {
        success = dvmCopyObjectArray(newArray, srcArray, dstElemClass);
    } else {
        success = dvmUnboxObjectArray(newArray, srcArray, dstElemClass);
    }
    if (!success) {
        ALOGE("Annotation array copy failed");
        dvmReleaseTrackedAlloc((Object*)newArray, self);
        newArray = NULL;
        goto bail;
    }

bail:
    /* replace old, return new */
    dvmReleaseTrackedAlloc(valueObj, self);
    return (Object*) newArray;
}

/*
 * Create a new AnnotationMember.
 *
 * "clazz" is the class on which the annotations are defined.  "pPtr"
 * points to a pointer into the annotation data.  "annoClass" is the
 * annotation's class.
 *
 * We extract the annotation's value, create a new AnnotationMember object,
 * and construct it.
 *
 * Returns NULL on failure; an exception may or may not be raised.
 */
static Object* createAnnotationMember(const ClassObject* clazz,
    const ClassObject* annoClass, const u1** pPtr)
{
    Thread* self = dvmThreadSelf();
    const DexFile* pDexFile = clazz->pDvmDex->pDexFile;
    StringObject* nameObj = NULL;
    Object* valueObj = NULL;
    Object* newMember = NULL;
    Object* methodObj = NULL;
    ClassObject* methodReturn = NULL;
    u4 elementNameIdx;
    const char* name;
    AnnotationValue avalue;
    JValue unused;
    bool failed = true;

    elementNameIdx = readUleb128(pPtr);

    if (!processAnnotationValue(clazz, pPtr, &avalue, kAllObjects)) {
        ALOGW("Failed processing annotation value");
        goto bail;
    }
    valueObj = (Object*)avalue.value.l;

    /* new member to hold the element */
    newMember = dvmAllocObject(gDvm.classLibcoreReflectAnnotationMember, ALLOC_DEFAULT);
    name = dexStringById(pDexFile, elementNameIdx);
    nameObj = dvmCreateStringFromCstr(name);

    /* find the method in the annotation class, given only the name */
    if (name != NULL) {
        Method* annoMeth = dvmFindVirtualMethodByName(annoClass, name);
        if (annoMeth == NULL) {
            ALOGW("WARNING: could not find annotation member %s in %s",
                name, annoClass->descriptor);
        } else {
            methodObj = dvmCreateReflectObjForMethod(annoClass, annoMeth);
            methodReturn = dvmGetBoxedReturnType(annoMeth);
        }
    }
    if (newMember == NULL || nameObj == NULL || methodObj == NULL ||
        methodReturn == NULL)
    {
        ALOGE("Failed creating annotation element (m=%p n=%p a=%p r=%p)",
            newMember, nameObj, methodObj, methodReturn);
        goto bail;
    }

    /* convert the return type, if necessary */
    valueObj = convertReturnType(valueObj, methodReturn);
    if (valueObj == NULL)
        goto bail;

    /* call 4-argument constructor */
    dvmCallMethod(self, gDvm.methOrgApacheHarmonyLangAnnotationAnnotationMember_init,
        newMember, &unused, nameObj, valueObj, methodReturn, methodObj);
    if (dvmCheckException(self)) {
        ALOGD("Failed constructing annotation element");
        goto bail;
    }

    failed = false;

bail:
    /* release tracked allocations */
    dvmReleaseTrackedAlloc(newMember, self);
    dvmReleaseTrackedAlloc((Object*)nameObj, self);
    dvmReleaseTrackedAlloc(valueObj, self);
    dvmReleaseTrackedAlloc(methodObj, self);
    if (failed)
        return NULL;
    else
        return newMember;
}

/*
 * Create a new Annotation object from what we find in the annotation item.
 *
 * "clazz" is the class on which the annotations are defined.  "pPtr"
 * points to a pointer into the annotation data.
 *
 * We use the AnnotationFactory class to create the annotation for us.  The
 * method we call is:
 *
 *  public static Annotation createAnnotation(
 *      Class<? extends Annotation> annotationType,
 *      AnnotationMember[] elements)
 *
 * Returns a new Annotation, which will NOT be in the local ref table and
 * not referenced elsewhere, so store it away soon.  On failure, returns NULL
 * with an exception raised.
 */
static Object* processEncodedAnnotation(const ClassObject* clazz,
    const u1** pPtr)
{
    Thread* self = dvmThreadSelf();
    Object* newAnno = NULL;
    ArrayObject* elementArray = NULL;
    const ClassObject* annoClass;
    const u1* ptr;
    u4 typeIdx, size, count;

    ptr = *pPtr;
    typeIdx = readUleb128(&ptr);
    size = readUleb128(&ptr);

    LOGVV("----- processEnc ptr=%p type=%d size=%d", ptr, typeIdx, size);

    annoClass = dvmDexGetResolvedClass(clazz->pDvmDex, typeIdx);
    if (annoClass == NULL) {
        annoClass = dvmResolveClass(clazz, typeIdx, true);
        if (annoClass == NULL) {
            ALOGE("Unable to resolve %s annotation class %d",
                clazz->descriptor, typeIdx);
            assert(dvmCheckException(self));
            dvmClearException(self);
            return NULL;
        }
    }

    ALOGV("----- processEnc ptr=%p [0x%06x]  typeIdx=%d size=%d class=%s",
        *pPtr, *pPtr - (u1*) clazz->pDvmDex->pDexFile->baseAddr,
        typeIdx, size, annoClass->descriptor);

    /*
     * Elements are parsed out and stored in an array.  The Harmony
     * constructor wants an array with just the declared elements --
     * default values get merged in later.
     */
    JValue result;

    if (size > 0) {
        elementArray = dvmAllocArrayByClass(gDvm.classLibcoreReflectAnnotationMemberArray,
                                            size, ALLOC_DEFAULT);
        if (elementArray == NULL) {
            ALOGE("failed to allocate annotation member array (%d elements)",
                size);
            goto bail;
        }
    }

    /*
     * "ptr" points to a byte stream with "size" occurrences of
     * annotation_element.
     */
    for (count = 0; count < size; count++) {
        Object* newMember = createAnnotationMember(clazz, annoClass, &ptr);
        if (newMember == NULL)
            goto bail;

        /* add it to the array */
        dvmSetObjectArrayElement(elementArray, count, newMember);
    }

    dvmCallMethod(self,
        gDvm.methOrgApacheHarmonyLangAnnotationAnnotationFactory_createAnnotation,
        NULL, &result, annoClass, elementArray);
    if (dvmCheckException(self)) {
        ALOGD("Failed creating an annotation");
        //dvmLogExceptionStackTrace();
        goto bail;
    }

    newAnno = (Object*)result.l;

bail:
    dvmReleaseTrackedAlloc((Object*) elementArray, NULL);
    *pPtr = ptr;
    if (newAnno == NULL && !dvmCheckException(self)) {
        /* make sure an exception is raised */
        dvmThrowRuntimeException("failure in processEncodedAnnotation");
    }
    return newAnno;
}

/*
 * Run through an annotation set and convert each entry into an Annotation
 * object.
 *
 * Returns an array of Annotation objects, or NULL with an exception raised
 * on alloc failure.
 */
static ArrayObject* processAnnotationSet(const ClassObject* clazz,
    const DexAnnotationSetItem* pAnnoSet, int visibility)
{
    DexFile* pDexFile = clazz->pDvmDex->pDexFile;
    const DexAnnotationItem* pAnnoItem;

    /* we need these later; make sure they're initialized */
    if (!dvmIsClassInitialized(gDvm.classLibcoreReflectAnnotationFactory))
        dvmInitClass(gDvm.classLibcoreReflectAnnotationFactory);
    if (!dvmIsClassInitialized(gDvm.classLibcoreReflectAnnotationMember))
        dvmInitClass(gDvm.classLibcoreReflectAnnotationMember);

    /* count up the number of visible elements */
    size_t count = 0;
    for (size_t i = 0; i < pAnnoSet->size; ++i) {
        pAnnoItem = dexGetAnnotationItem(pDexFile, pAnnoSet, i);
        if (pAnnoItem->visibility == visibility) {
            count++;
        }
    }

    ArrayObject* annoArray = dvmAllocArrayByClass(gDvm.classJavaLangAnnotationAnnotationArray,
                                                  count, ALLOC_DEFAULT);
    if (annoArray == NULL) {
        return NULL;
    }

    /*
     * Generate Annotation objects.  We must put them into the array
     * immediately (or add them to the tracked ref table).
     * We may not be able to resolve all annotations, and should just
     * ignore those we can't.
     */
    u4 dstIndex = 0;
    for (int i = 0; i < (int) pAnnoSet->size; i++) {
        pAnnoItem = dexGetAnnotationItem(pDexFile, pAnnoSet, i);
        if (pAnnoItem->visibility != visibility)
            continue;
        const u1* ptr = pAnnoItem->annotation;
        Object *anno = processEncodedAnnotation(clazz, &ptr);
        if (anno != NULL) {
            dvmSetObjectArrayElement(annoArray, dstIndex, anno);
            ++dstIndex;
        }
    }

    // If we got as many as we expected, we're done...
    if (dstIndex == count) {
        return annoArray;
    }

    // ...otherwise we need to trim the trailing nulls.
    ArrayObject* trimmedArray = dvmAllocArrayByClass(gDvm.classJavaLangAnnotationAnnotationArray,
                                                     dstIndex, ALLOC_DEFAULT);
    if (trimmedArray == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < dstIndex; ++i) {
        Object** src = (Object**)(void*) annoArray->contents;
        dvmSetObjectArrayElement(trimmedArray, i, src[i]);
    }
    dvmReleaseTrackedAlloc((Object*) annoArray, NULL);
    return trimmedArray;
}

/*
 * Return the annotation item of the specified type in the annotation set, or
 * NULL if the set contains no annotation of that type.
 */
static const DexAnnotationItem* getAnnotationItemFromAnnotationSet(
        const ClassObject* clazz, const DexAnnotationSetItem* pAnnoSet,
        int visibility, const ClassObject* annotationClazz)
{
    DexFile* pDexFile = clazz->pDvmDex->pDexFile;
    const DexAnnotationItem* pAnnoItem;
    int i;
    const ClassObject* annoClass;
    const u1* ptr;
    u4 typeIdx;

    /* we need these later; make sure they're initialized */
    if (!dvmIsClassInitialized(gDvm.classLibcoreReflectAnnotationFactory))
        dvmInitClass(gDvm.classLibcoreReflectAnnotationFactory);
    if (!dvmIsClassInitialized(gDvm.classLibcoreReflectAnnotationMember))
        dvmInitClass(gDvm.classLibcoreReflectAnnotationMember);

    for (i = 0; i < (int) pAnnoSet->size; i++) {
        pAnnoItem = dexGetAnnotationItem(pDexFile, pAnnoSet, i);
        if (pAnnoItem->visibility != visibility)
            continue;

        ptr = pAnnoItem->annotation;
        typeIdx = readUleb128(&ptr);

        annoClass = dvmDexGetResolvedClass(clazz->pDvmDex, typeIdx);
        if (annoClass == NULL) {
            annoClass = dvmResolveClass(clazz, typeIdx, true);
            if (annoClass == NULL) {
                ALOGE("Unable to resolve %s annotation class %d",
                      clazz->descriptor, typeIdx);
                Thread* self = dvmThreadSelf();
                assert(dvmCheckException(self));
                dvmClearException(self);
                continue;
            }
        }

        if (annoClass == annotationClazz) {
            return pAnnoItem;
        }
    }

    return NULL;
}

/*
 * Return the Annotation object of the specified type in the annotation set, or
 * NULL if the set contains no annotation of that type.
 */
static Object* getAnnotationObjectFromAnnotationSet(const ClassObject* clazz,
        const DexAnnotationSetItem* pAnnoSet, int visibility,
        const ClassObject* annotationClazz)
{
    const DexAnnotationItem* pAnnoItem = getAnnotationItemFromAnnotationSet(
            clazz, pAnnoSet, visibility, annotationClazz);
    if (pAnnoItem == NULL) {
        return NULL;
    }
    const u1* ptr = pAnnoItem->annotation;
    return processEncodedAnnotation(clazz, &ptr);
}

/*
 * ===========================================================================
 *      Skipping and scanning
 * ===========================================================================
 */

/*
 * Skip past an annotation value.
 *
 * "clazz" is the class on which the annotations are defined.
 *
 * Returns "true" on success, "false" on parsing failure.
 */
static bool skipAnnotationValue(const ClassObject* clazz, const u1** pPtr)
{
    const u1* ptr = *pPtr;
    u1 valueType, valueArg;
    int width;

    valueType = *ptr++;
    valueArg = valueType >> kDexAnnotationValueArgShift;
    width = valueArg + 1;       /* assume */

    ALOGV("----- type is 0x%02x %d, ptr=%p [0x%06x]",
        valueType & kDexAnnotationValueTypeMask, valueArg, ptr-1,
        (ptr-1) - (u1*)clazz->pDvmDex->pDexFile->baseAddr);

    switch (valueType & kDexAnnotationValueTypeMask) {
    case kDexAnnotationByte:        break;
    case kDexAnnotationShort:       break;
    case kDexAnnotationChar:        break;
    case kDexAnnotationInt:         break;
    case kDexAnnotationLong:        break;
    case kDexAnnotationFloat:       break;
    case kDexAnnotationDouble:      break;
    case kDexAnnotationString:      break;
    case kDexAnnotationType:        break;
    case kDexAnnotationMethod:      break;
    case kDexAnnotationField:       break;
    case kDexAnnotationEnum:        break;

    case kDexAnnotationArray:
        /* encoded_array format */
        {
            u4 size = readUleb128(&ptr);
            while (size--) {
                if (!skipAnnotationValue(clazz, &ptr))
                    return false;
            }
        }
        width = 0;
        break;
    case kDexAnnotationAnnotation:
        /* encoded_annotation format */
        if (!skipEncodedAnnotation(clazz, &ptr))
            return false;
        width = 0;
        break;
    case kDexAnnotationBoolean:
    case kDexAnnotationNull:
        width = 0;
        break;
    default:
        ALOGE("Bad annotation element value byte 0x%02x", valueType);
        assert(false);
        return false;
    }

    ptr += width;

    *pPtr = ptr;
    return true;
}

/*
 * Skip past an encoded annotation.  Mainly useful for annotations embedded
 * in other annotations.
 */
static bool skipEncodedAnnotation(const ClassObject* clazz, const u1** pPtr)
{
    const u1* ptr;
    u4 size;

    ptr = *pPtr;
    (void) readUleb128(&ptr);
    size = readUleb128(&ptr);

    /*
     * "ptr" points to a byte stream with "size" occurrences of
     * annotation_element.
     */
    while (size--) {
        (void) readUleb128(&ptr);

        if (!skipAnnotationValue(clazz, &ptr))
            return false;
    }

    *pPtr = ptr;
    return true;
}


/*
 * Compare the name of the class in the DEX file to the supplied descriptor.
 * Return value is equivalent to strcmp.
 */
static int compareClassDescriptor(DexFile* pDexFile, u4 typeIdx,
    const char* descriptor)
{
    const char* str = dexStringByTypeIdx(pDexFile, typeIdx);

    return strcmp(str, descriptor);
}

/*
 * Search through the annotation set for an annotation with a matching
 * descriptor.
 *
 * Comparing the string descriptor is slower than comparing an integer class
 * index.  If annotation lists are expected to be long, we could look up
 * the class' index by name from the DEX file, rather than doing a class
 * lookup and string compare on each entry.  (Note the index will be
 * different for each DEX file, so we can't cache annotation class indices
 * globally.)
 */
static const DexAnnotationItem* searchAnnotationSet(const ClassObject* clazz,
    const DexAnnotationSetItem* pAnnoSet, const char* descriptor,
    int visibility)
{
    DexFile* pDexFile = clazz->pDvmDex->pDexFile;
    const DexAnnotationItem* result = NULL;
    u4 typeIdx;
    int i;

    //printf("##### searchAnnotationSet %s %d\n", descriptor, visibility);

    for (i = 0; i < (int) pAnnoSet->size; i++) {
        const DexAnnotationItem* pAnnoItem;

        pAnnoItem = dexGetAnnotationItem(pDexFile, pAnnoSet, i);
        if (pAnnoItem->visibility != visibility)
            continue;
        const u1* ptr = pAnnoItem->annotation;
        typeIdx = readUleb128(&ptr);

        if (compareClassDescriptor(pDexFile, typeIdx, descriptor) == 0) {
            //printf("#####  match on %x/%p at %d\n", typeIdx, pDexFile, i);
            result = pAnnoItem;
            break;
        }
    }

    return result;
}

/*
 * Find an annotation value in the annotation_item whose name matches "name".
 * A pointer to the annotation_value is returned, or NULL if it's not found.
 */
static const u1* searchEncodedAnnotation(const ClassObject* clazz,
    const u1* ptr, const char* name)
{
    DexFile* pDexFile = clazz->pDvmDex->pDexFile;
    u4 typeIdx, size;

    typeIdx = readUleb128(&ptr);
    size = readUleb128(&ptr);
    //printf("#####   searching ptr=%p type=%u size=%u\n", ptr, typeIdx, size);

    while (size--) {
        u4 elementNameIdx;
        const char* elemName;

        elementNameIdx = readUleb128(&ptr);
        elemName = dexStringById(pDexFile, elementNameIdx);
        if (strcmp(name, elemName) == 0) {
            //printf("#####   item match on %s\n", name);
            return ptr;     /* points to start of value */
        }

        skipAnnotationValue(clazz, &ptr);
    }

    //printf("#####   no item match on %s\n", name);
    return NULL;
}

#define GAV_FAILED  ((Object*) 0x10000001)

/*
 * Extract an encoded annotation value from the field specified by "annoName".
 *
 * "expectedType" is an annotation value type, e.g. kDexAnnotationString.
 * "debugAnnoName" is only used in debug messages.
 *
 * Returns GAV_FAILED on failure.  If an allocation failed, an exception
 * will be raised.
 */
static Object* getAnnotationValue(const ClassObject* clazz,
    const DexAnnotationItem* pAnnoItem, const char* annoName,
    int expectedType, const char* debugAnnoName)
{
    const u1* ptr;
    AnnotationValue avalue;

    /* find the annotation */
    ptr = searchEncodedAnnotation(clazz, pAnnoItem->annotation, annoName);
    if (ptr == NULL) {
        ALOGW("%s annotation lacks '%s' member", debugAnnoName, annoName);
        return GAV_FAILED;
    }

    if (!processAnnotationValue(clazz, &ptr, &avalue, kAllObjects))
        return GAV_FAILED;

    /* make sure it has the expected format */
    if (avalue.type != expectedType) {
        ALOGW("%s %s has wrong type (0x%02x, expected 0x%02x)",
            debugAnnoName, annoName, avalue.type, expectedType);
        return GAV_FAILED;
    }

    return (Object*)avalue.value.l;
}


/*
 * Find the Signature attribute and extract its value.  (Signatures can
 * be found in annotations on classes, constructors, methods, and fields.)
 *
 * Caller must call dvmReleaseTrackedAlloc().
 *
 * Returns NULL if not found.  On memory alloc failure, returns NULL with an
 * exception raised.
 */
static ArrayObject* getSignatureValue(const ClassObject* clazz,
    const DexAnnotationSetItem* pAnnoSet)
{
    const DexAnnotationItem* pAnnoItem;
    Object* obj;

    pAnnoItem = searchAnnotationSet(clazz, pAnnoSet, kDescrSignature,
        kDexVisibilitySystem);
    if (pAnnoItem == NULL)
        return NULL;

    /*
     * The Signature annotation has one member, "String value".
     */
    obj = getAnnotationValue(clazz, pAnnoItem, "value", kDexAnnotationArray,
            "Signature");
    if (obj == GAV_FAILED)
        return NULL;
    assert(obj->clazz == gDvm.classJavaLangObjectArray);

    return (ArrayObject*)obj;
}


/*
 * ===========================================================================
 *      Class
 * ===========================================================================
 */

/*
 * Find the DexAnnotationSetItem for this class.
 */
static const DexAnnotationSetItem* findAnnotationSetForClass(
    const ClassObject* clazz)
{
    DexFile* pDexFile;
    const DexAnnotationsDirectoryItem* pAnnoDir;

    if (clazz->pDvmDex == NULL)         /* generated class (Proxy, array) */
        return NULL;

    pDexFile = clazz->pDvmDex->pDexFile;
    pAnnoDir = getAnnoDirectory(pDexFile, clazz);
    if (pAnnoDir != NULL)
        return dexGetClassAnnotationSet(pDexFile, pAnnoDir);
    else
        return NULL;
}

/*
 * Return an array of Annotation objects for the class.  Returns an empty
 * array if there are no annotations.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 *
 * On allocation failure, this returns NULL with an exception raised.
 */
ArrayObject* dvmGetClassAnnotations(const ClassObject* clazz)
{
    ArrayObject* annoArray;
    const DexAnnotationSetItem* pAnnoSet = NULL;

    pAnnoSet = findAnnotationSetForClass(clazz);
    if (pAnnoSet == NULL) {
        /* no annotations for anything in class, or no class annotations */
        annoArray = emptyAnnoArray();
    } else {
        annoArray = processAnnotationSet(clazz, pAnnoSet,
                        kDexVisibilityRuntime);
    }

    return annoArray;
}

/*
 * Returns the annotation or NULL if it doesn't exist.
 */
Object* dvmGetClassAnnotation(const ClassObject* clazz,
        const ClassObject* annotationClazz)
{
    const DexAnnotationSetItem* pAnnoSet = findAnnotationSetForClass(clazz);
    if (pAnnoSet == NULL) {
        return NULL;
    }
    return getAnnotationObjectFromAnnotationSet(clazz, pAnnoSet,
            kDexVisibilityRuntime, annotationClazz);
}

/*
 * Returns true if the annotation exists.
 */
bool dvmIsClassAnnotationPresent(const ClassObject* clazz,
        const ClassObject* annotationClazz)
{
    const DexAnnotationSetItem* pAnnoSet = findAnnotationSetForClass(clazz);
    if (pAnnoSet == NULL) {
        return NULL;
    }
    const DexAnnotationItem* pAnnoItem = getAnnotationItemFromAnnotationSet(
            clazz, pAnnoSet, kDexVisibilityRuntime, annotationClazz);
    return (pAnnoItem != NULL);
}

/*
 * Retrieve the Signature annotation, if any.  Returns NULL if no signature
 * exists.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
ArrayObject* dvmGetClassSignatureAnnotation(const ClassObject* clazz)
{
    ArrayObject* signature = NULL;
    const DexAnnotationSetItem* pAnnoSet;

    pAnnoSet = findAnnotationSetForClass(clazz);
    if (pAnnoSet != NULL)
        signature = getSignatureValue(clazz, pAnnoSet);

    return signature;
}

/*
 * Get the EnclosingMethod attribute from an annotation.  Returns a Method
 * object, or NULL.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
Object* dvmGetEnclosingMethod(const ClassObject* clazz)
{
    const DexAnnotationItem* pAnnoItem;
    const DexAnnotationSetItem* pAnnoSet;
    Object* obj;

    pAnnoSet = findAnnotationSetForClass(clazz);
    if (pAnnoSet == NULL)
        return NULL;

    pAnnoItem = searchAnnotationSet(clazz, pAnnoSet, kDescrEnclosingMethod,
        kDexVisibilitySystem);
    if (pAnnoItem == NULL)
        return NULL;

    /*
     * The EnclosingMethod annotation has one member, "Method value".
     */
    obj = getAnnotationValue(clazz, pAnnoItem, "value", kDexAnnotationMethod,
            "EnclosingMethod");
    if (obj == GAV_FAILED)
        return NULL;
    assert(obj->clazz == gDvm.classJavaLangReflectConstructor ||
           obj->clazz == gDvm.classJavaLangReflectMethod);

    return obj;
}

/*
 * Find a class' enclosing class.  We return what we find in the
 * EnclosingClass attribute.
 *
 * Returns a Class object, or NULL.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
ClassObject* dvmGetDeclaringClass(const ClassObject* clazz)
{
    const DexAnnotationItem* pAnnoItem;
    const DexAnnotationSetItem* pAnnoSet;
    Object* obj;

    pAnnoSet = findAnnotationSetForClass(clazz);
    if (pAnnoSet == NULL)
        return NULL;

    pAnnoItem = searchAnnotationSet(clazz, pAnnoSet, kDescrEnclosingClass,
        kDexVisibilitySystem);
    if (pAnnoItem == NULL)
        return NULL;

    /*
     * The EnclosingClass annotation has one member, "Class value".
     */
    obj = getAnnotationValue(clazz, pAnnoItem, "value", kDexAnnotationType,
            "EnclosingClass");
    if (obj == GAV_FAILED)
        return NULL;

    assert(dvmIsClassObject(obj));
    return (ClassObject*)obj;
}

/*
 * Find a class' enclosing class.  We first search for an EnclosingClass
 * attribute, and if that's not found we look for an EnclosingMethod.
 *
 * Returns a Class object, or NULL.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
ClassObject* dvmGetEnclosingClass(const ClassObject* clazz)
{
    const DexAnnotationItem* pAnnoItem;
    const DexAnnotationSetItem* pAnnoSet;
    Object* obj;

    pAnnoSet = findAnnotationSetForClass(clazz);
    if (pAnnoSet == NULL)
        return NULL;

    pAnnoItem = searchAnnotationSet(clazz, pAnnoSet, kDescrEnclosingClass,
        kDexVisibilitySystem);
    if (pAnnoItem != NULL) {
        /*
         * The EnclosingClass annotation has one member, "Class value".
         */
        obj = getAnnotationValue(clazz, pAnnoItem, "value", kDexAnnotationType,
                "EnclosingClass");
        if (obj != GAV_FAILED) {
            assert(dvmIsClassObject(obj));
            return (ClassObject*)obj;
        }
    }

    /*
     * That didn't work.  Look for an EnclosingMethod.
     *
     * We could create a java.lang.reflect.Method object and extract the
     * declaringClass from it, but that's more work than we want to do.
     * Instead, we find the "value" item and parse the index out ourselves.
     */
    pAnnoItem = searchAnnotationSet(clazz, pAnnoSet, kDescrEnclosingMethod,
        kDexVisibilitySystem);
    if (pAnnoItem == NULL)
        return NULL;

    /* find the value member */
    const u1* ptr;
    ptr = searchEncodedAnnotation(clazz, pAnnoItem->annotation, "value");
    if (ptr == NULL) {
        ALOGW("EnclosingMethod annotation lacks 'value' member");
        return NULL;
    }

    /* parse it, verify the type */
    AnnotationValue avalue;
    if (!processAnnotationValue(clazz, &ptr, &avalue, kAllRaw)) {
        ALOGW("EnclosingMethod parse failed");
        return NULL;
    }
    if (avalue.type != kDexAnnotationMethod) {
        ALOGW("EnclosingMethod value has wrong type (0x%02x, expected 0x%02x)",
            avalue.type, kDexAnnotationMethod);
        return NULL;
    }

    /* pull out the method index and resolve the method */
    Method* meth = resolveAmbiguousMethod(clazz, avalue.value.i);
    if (meth == NULL)
        return NULL;

    ClassObject* methClazz = meth->clazz;
    dvmAddTrackedAlloc((Object*) methClazz, NULL);      // balance the Release
    return methClazz;
}

/*
 * Get the EnclosingClass attribute from an annotation.  If found, returns
 * "true".  A String with the original name of the class and the original
 * access flags are returned through the arguments.  (The name will be NULL
 * for an anonymous inner class.)
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
bool dvmGetInnerClass(const ClassObject* clazz, StringObject** pName,
    int* pAccessFlags)
{
    const DexAnnotationItem* pAnnoItem;
    const DexAnnotationSetItem* pAnnoSet;

    pAnnoSet = findAnnotationSetForClass(clazz);
    if (pAnnoSet == NULL)
        return false;

    pAnnoItem = searchAnnotationSet(clazz, pAnnoSet, kDescrInnerClass,
        kDexVisibilitySystem);
    if (pAnnoItem == NULL)
        return false;

    /*
     * The InnerClass annotation has two members, "String name" and
     * "int accessFlags".  We don't want to get the access flags as an
     * Integer, so we process that as a simple value.
     */
    const u1* ptr;
    ptr = searchEncodedAnnotation(clazz, pAnnoItem->annotation, "name");
    if (ptr == NULL) {
        ALOGW("InnerClass annotation lacks 'name' member");
        return false;
    }

    /* parse it into an Object */
    AnnotationValue avalue;
    if (!processAnnotationValue(clazz, &ptr, &avalue, kAllObjects)) {
        ALOGD("processAnnotationValue failed on InnerClass member 'name'");
        return false;
    }

    /* make sure it has the expected format */
    if (avalue.type != kDexAnnotationNull &&
        avalue.type != kDexAnnotationString)
    {
        ALOGW("InnerClass name has bad type (0x%02x, expected STRING or NULL)",
            avalue.type);
        return false;
    }

    *pName = (StringObject*) avalue.value.l;
    assert(*pName == NULL || (*pName)->clazz == gDvm.classJavaLangString);

    ptr = searchEncodedAnnotation(clazz, pAnnoItem->annotation, "accessFlags");
    if (ptr == NULL) {
        ALOGW("InnerClass annotation lacks 'accessFlags' member");
        return false;
    }

    /* parse it, verify the type */
    if (!processAnnotationValue(clazz, &ptr, &avalue, kAllRaw)) {
        ALOGW("InnerClass accessFlags parse failed");
        return false;
    }
    if (avalue.type != kDexAnnotationInt) {
        ALOGW("InnerClass value has wrong type (0x%02x, expected 0x%02x)",
            avalue.type, kDexAnnotationInt);
        return false;
    }

    *pAccessFlags = avalue.value.i;

    return true;
}

/*
 * Extract an array of Class objects from the MemberClasses annotation
 * for this class.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 *
 * Returns NULL if we don't find any member classes.
 */
ArrayObject* dvmGetDeclaredClasses(const ClassObject* clazz)
{
    const DexAnnotationSetItem* pAnnoSet;
    const DexAnnotationItem* pAnnoItem;
    Object* obj;

    pAnnoSet = findAnnotationSetForClass(clazz);
    if (pAnnoSet == NULL)
        return NULL;

    pAnnoItem = searchAnnotationSet(clazz, pAnnoSet, kDescrMemberClasses,
        kDexVisibilitySystem);
    if (pAnnoItem == NULL)
        return NULL;

    /*
     * The MemberClasses annotation has one member, "Class[] value".
     */
    obj = getAnnotationValue(clazz, pAnnoItem, "value",
            kDexAnnotationArray, "MemberClasses");
    if (obj == GAV_FAILED)
        return NULL;
    assert(dvmIsArray((ArrayObject*)obj));
    obj = convertReturnType(obj, gDvm.classJavaLangClassArray);
    return (ArrayObject*)obj;
}


/*
 * ===========================================================================
 *      Method (and Constructor)
 * ===========================================================================
 */

/*
 * Compare the attributes (class name, method name, method signature) of
 * the specified method to "method".
 */
static int compareMethodStr(DexFile* pDexFile, u4 methodIdx,
    const Method* method)
{
    const DexMethodId* pMethodId = dexGetMethodId(pDexFile, methodIdx);
    const char* str = dexStringByTypeIdx(pDexFile, pMethodId->classIdx);
    int result = strcmp(str, method->clazz->descriptor);

    if (result == 0) {
        str = dexStringById(pDexFile, pMethodId->nameIdx);
        result = strcmp(str, method->name);
        if (result == 0) {
            DexProto proto;
            dexProtoSetFromMethodId(&proto, pDexFile, pMethodId);
            result = dexProtoCompare(&proto, &method->prototype);
        }
    }

    return result;
}

/*
 * Given a method, determine the method's index.
 *
 * We could simply store this in the Method*, but that would cost 4 bytes
 * per method.  Instead we plow through the DEX data.
 *
 * We have two choices: look through the class method data, or look through
 * the global method_ids table.  The former is awkward because the method
 * could have been defined in a superclass or interface.  The latter works
 * out reasonably well because it's in sorted order, though we're still left
 * doing a fair number of string comparisons.
 */
u4 dvmGetMethodIdx(const Method* method)
{
    if (method->clazz->pDvmDex == NULL) return 0;

    DexFile* pDexFile = method->clazz->pDvmDex->pDexFile;
    u4 hi = pDexFile->pHeader->methodIdsSize -1;
    u4 lo = 0;
    u4 cur;

    while (hi >= lo) {
        int cmp;
        cur = (lo + hi) / 2;

        cmp = compareMethodStr(pDexFile, cur, method);
        if (cmp < 0) {
            lo = cur + 1;
        } else if (cmp > 0) {
            hi = cur - 1;
        } else {
            break;
        }
    }

    if (hi < lo) {
        /* this should be impossible -- the method came out of this DEX */
        char* desc = dexProtoCopyMethodDescriptor(&method->prototype);
        ALOGE("Unable to find method %s.%s %s in DEX file!",
            method->clazz->descriptor, method->name, desc);
        free(desc);
        dvmAbort();
    }

    return cur;
}

/*
 * Find the DexAnnotationSetItem for this method.
 *
 * Returns NULL if none found.
 */
static const DexAnnotationSetItem* findAnnotationSetForMethod(
    const Method* method)
{
    ClassObject* clazz = method->clazz;
    DexFile* pDexFile;
    const DexAnnotationsDirectoryItem* pAnnoDir;
    const DexMethodAnnotationsItem* pMethodList;
    const DexAnnotationSetItem* pAnnoSet = NULL;

    if (clazz->pDvmDex == NULL)         /* generated class (Proxy, array) */
        return NULL;
    pDexFile = clazz->pDvmDex->pDexFile;

    pAnnoDir = getAnnoDirectory(pDexFile, clazz);
    if (pAnnoDir != NULL) {
        pMethodList = dexGetMethodAnnotations(pDexFile, pAnnoDir);
        if (pMethodList != NULL) {
            /*
             * Run through the list and find a matching method.  We compare the
             * method ref indices in the annotation list with the method's DEX
             * method_idx value.
             *
             * TODO: use a binary search for long lists
             *
             * Alternate approach: for each entry in the annotations list,
             * find the method definition in the DEX file and perform string
             * comparisons on class name, method name, and signature.
             */
            u4 methodIdx = dvmGetMethodIdx(method);
            u4 count = dexGetMethodAnnotationsSize(pDexFile, pAnnoDir);
            u4 idx;

            for (idx = 0; idx < count; idx++) {
                if (pMethodList[idx].methodIdx == methodIdx) {
                    /* found! */
                    pAnnoSet = dexGetMethodAnnotationSetItem(pDexFile,
                                    &pMethodList[idx]);
                    break;
                }
            }
        }
    }

    return pAnnoSet;
}

/*
 * Return an array of Annotation objects for the method.  Returns an empty
 * array if there are no annotations.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 *
 * On allocation failure, this returns NULL with an exception raised.
 */
ArrayObject* dvmGetMethodAnnotations(const Method* method)
{
    ClassObject* clazz = method->clazz;
    const DexAnnotationSetItem* pAnnoSet;
    ArrayObject* annoArray = NULL;

    pAnnoSet = findAnnotationSetForMethod(method);
    if (pAnnoSet == NULL) {
        /* no matching annotations found */
        annoArray = emptyAnnoArray();
    } else {
        annoArray = processAnnotationSet(clazz, pAnnoSet,kDexVisibilityRuntime);
    }

    return annoArray;
}

/*
 * Returns the annotation or NULL if it doesn't exist.
 */
Object* dvmGetMethodAnnotation(const ClassObject* clazz, const Method* method,
        const ClassObject* annotationClazz)
{
    const DexAnnotationSetItem* pAnnoSet = findAnnotationSetForMethod(method);
    if (pAnnoSet == NULL) {
        return NULL;
    }
    return getAnnotationObjectFromAnnotationSet(clazz, pAnnoSet,
            kDexVisibilityRuntime, annotationClazz);
}

/*
 * Returns true if the annotation exists.
 */
bool dvmIsMethodAnnotationPresent(const ClassObject* clazz,
        const Method* method, const ClassObject* annotationClazz)
{
    const DexAnnotationSetItem* pAnnoSet = findAnnotationSetForMethod(method);
    if (pAnnoSet == NULL) {
        return NULL;
    }
    const DexAnnotationItem* pAnnoItem = getAnnotationItemFromAnnotationSet(
            clazz, pAnnoSet, kDexVisibilityRuntime, annotationClazz);
    return (pAnnoItem != NULL);
}

/*
 * Retrieve the Signature annotation, if any.  Returns NULL if no signature
 * exists.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
ArrayObject* dvmGetMethodSignatureAnnotation(const Method* method)
{
    ClassObject* clazz = method->clazz;
    const DexAnnotationSetItem* pAnnoSet;
    ArrayObject* signature = NULL;

    pAnnoSet = findAnnotationSetForMethod(method);
    if (pAnnoSet != NULL)
        signature = getSignatureValue(clazz, pAnnoSet);

    return signature;
}

/*
 * Extract an array of exception classes from the "system" annotation list
 * for this method.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 *
 * Returns NULL if we don't find any exceptions for this method.
 */
ArrayObject* dvmGetMethodThrows(const Method* method)
{
    ClassObject* clazz = method->clazz;
    const DexAnnotationSetItem* pAnnoSet;
    const DexAnnotationItem* pAnnoItem;

    /* find the set for this method */
    pAnnoSet = findAnnotationSetForMethod(method);
    if (pAnnoSet == NULL)
        return NULL;        /* nothing for this method */

    /* find the "Throws" annotation, if any */
    pAnnoItem = searchAnnotationSet(clazz, pAnnoSet, kDescrThrows,
        kDexVisibilitySystem);
    if (pAnnoItem == NULL)
        return NULL;        /* no Throws */

    /*
     * The Throws annotation has one member, "Class[] value".
     */
    Object* obj = getAnnotationValue(clazz, pAnnoItem, "value",
        kDexAnnotationArray, "Throws");
    if (obj == GAV_FAILED)
        return NULL;
    assert(dvmIsArray((ArrayObject*)obj));
    obj = convertReturnType(obj, gDvm.classJavaLangClassArray);
    return (ArrayObject*)obj;
}

/*
 * Given an Annotation's method, find the default value, if any.
 *
 * If this is a CLASS annotation, and we can't find a match for the
 * default class value, we need to throw a TypeNotPresentException.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
Object* dvmGetAnnotationDefaultValue(const Method* method)
{
    const ClassObject* clazz = method->clazz;
    DexFile* pDexFile = clazz->pDvmDex->pDexFile;
    const DexAnnotationsDirectoryItem* pAnnoDir;
    const DexAnnotationSetItem* pAnnoSet = NULL;

    /*
     * The method's declaring class (the annotation) will have an
     * AnnotationDefault "system" annotation associated with it if any
     * of its methods have default values.  Start by finding the
     * DexAnnotationItem associated with the class.
     */
    pAnnoDir = getAnnoDirectory(pDexFile, clazz);
    if (pAnnoDir != NULL)
        pAnnoSet = dexGetClassAnnotationSet(pDexFile, pAnnoDir);
    if (pAnnoSet == NULL) {
        /* no annotations for anything in class, or no class annotations */
        return NULL;
    }

    /* find the "AnnotationDefault" annotation, if any */
    const DexAnnotationItem* pAnnoItem;
    pAnnoItem = searchAnnotationSet(clazz, pAnnoSet, kDescrAnnotationDefault,
        kDexVisibilitySystem);
    if (pAnnoItem == NULL) {
        /* no default values for any member in this annotation */
        //printf("##### no default annotations for %s.%s\n",
        //    method->clazz->descriptor, method->name);
        return NULL;
    }

    /*
     * The AnnotationDefault annotation has one member, "Annotation value".
     * We need to pull that out.
     */
    const u1* ptr;
    ptr = searchEncodedAnnotation(clazz, pAnnoItem->annotation, "value");
    if (ptr == NULL) {
        ALOGW("AnnotationDefault annotation lacks 'value'");
        return NULL;
    }
    if ((*ptr & kDexAnnotationValueTypeMask) != kDexAnnotationAnnotation) {
        ALOGW("AnnotationDefault value has wrong type (0x%02x)",
            *ptr & kDexAnnotationValueTypeMask);
        return NULL;
    }

    /*
     * The value_type byte for VALUE_ANNOTATION is followed by
     * encoded_annotation data.  We want to scan through it to find an
     * entry whose name matches our method name.
     */
    ptr++;
    ptr = searchEncodedAnnotation(clazz, ptr, method->name);
    if (ptr == NULL)
        return NULL;        /* no default annotation for this method */

    /* got it, pull it out */
    AnnotationValue avalue;
    if (!processAnnotationValue(clazz, &ptr, &avalue, kAllObjects)) {
        ALOGD("processAnnotationValue failed on default for '%s'",
            method->name);
        return NULL;
    }

    /* convert the return type, if necessary */
    ClassObject* methodReturn = dvmGetBoxedReturnType(method);
    Object* obj = (Object*)avalue.value.l;
    obj = convertReturnType(obj, methodReturn);

    return obj;
}


/*
 * ===========================================================================
 *      Field
 * ===========================================================================
 */

/*
 * Compare the attributes (class name, field name, field signature) of
 * the specified field to "field".
 */
static int compareFieldStr(DexFile* pDexFile, u4 idx, const Field* field)
{
    const DexFieldId* pFieldId = dexGetFieldId(pDexFile, idx);
    const char* str = dexStringByTypeIdx(pDexFile, pFieldId->classIdx);
    int result = strcmp(str, field->clazz->descriptor);

    if (result == 0) {
        str = dexStringById(pDexFile, pFieldId->nameIdx);
        result = strcmp(str, field->name);
        if (result == 0) {
            str = dexStringByTypeIdx(pDexFile, pFieldId->typeIdx);
            result = strcmp(str, field->signature);
        }
    }

    return result;
}

/*
 * Given a field, determine the field's index.
 *
 * This has the same tradeoffs as dvmGetMethodIdx.
 */
u4 dvmGetFieldIdx(const Field* field)
{
    if (field->clazz->pDvmDex == NULL) return 0;

    DexFile* pDexFile = field->clazz->pDvmDex->pDexFile;
    u4 hi = pDexFile->pHeader->fieldIdsSize -1;
    u4 lo = 0;
    u4 cur;

    while (hi >= lo) {
        int cmp;
        cur = (lo + hi) / 2;

        cmp = compareFieldStr(pDexFile, cur, field);
        if (cmp < 0) {
            lo = cur + 1;
        } else if (cmp > 0) {
            hi = cur - 1;
        } else {
            break;
        }
    }

    if (hi < lo) {
        /* this should be impossible -- the field came out of this DEX */
        ALOGE("Unable to find field %s.%s %s in DEX file!",
            field->clazz->descriptor, field->name, field->signature);
        dvmAbort();
    }

    return cur;
}

/*
 * Find the DexAnnotationSetItem for this field.
 *
 * Returns NULL if none found.
 */
static const DexAnnotationSetItem* findAnnotationSetForField(const Field* field)
{
    ClassObject* clazz = field->clazz;
    DvmDex* pDvmDex = clazz->pDvmDex;
    if (pDvmDex == NULL) {
        return NULL;
    }

    DexFile* pDexFile = pDvmDex->pDexFile;

    const DexAnnotationsDirectoryItem* pAnnoDir = getAnnoDirectory(pDexFile, clazz);
    if (pAnnoDir == NULL) {
        return NULL;
    }

    const DexFieldAnnotationsItem* pFieldList = dexGetFieldAnnotations(pDexFile, pAnnoDir);
    if (pFieldList == NULL) {
        return NULL;
    }

    /*
     * Run through the list and find a matching field.  We compare the
     * field ref indices in the annotation list with the field's DEX
     * field_idx value.
     *
     * TODO: use a binary search for long lists
     *
     * Alternate approach: for each entry in the annotations list,
     * find the field definition in the DEX file and perform string
     * comparisons on class name, field name, and signature.
     */
    u4 fieldIdx = dvmGetFieldIdx(field);
    u4 count = dexGetFieldAnnotationsSize(pDexFile, pAnnoDir);
    u4 idx;

    for (idx = 0; idx < count; idx++) {
        if (pFieldList[idx].fieldIdx == fieldIdx) {
            /* found! */
            return dexGetFieldAnnotationSetItem(pDexFile, &pFieldList[idx]);
        }
    }

    return NULL;
}

/*
 * Return an array of Annotation objects for the field.  Returns an empty
 * array if there are no annotations.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 *
 * On allocation failure, this returns NULL with an exception raised.
 */
ArrayObject* dvmGetFieldAnnotations(const Field* field)
{
    ClassObject* clazz = field->clazz;
    ArrayObject* annoArray = NULL;
    const DexAnnotationSetItem* pAnnoSet = NULL;

    pAnnoSet = findAnnotationSetForField(field);
    if (pAnnoSet == NULL) {
        /* no matching annotations found */
        annoArray = emptyAnnoArray();
    } else {
        annoArray = processAnnotationSet(clazz, pAnnoSet,
                        kDexVisibilityRuntime);
    }

    return annoArray;
}

/*
 * Returns the annotation or NULL if it doesn't exist.
 */
Object* dvmGetFieldAnnotation(const ClassObject* clazz, const Field* field,
        const ClassObject* annotationClazz)
{
    const DexAnnotationSetItem* pAnnoSet = findAnnotationSetForField(field);
    if (pAnnoSet == NULL) {
        return NULL;
    }
    return getAnnotationObjectFromAnnotationSet(clazz, pAnnoSet,
            kDexVisibilityRuntime, annotationClazz);
}

/*
 * Returns true if the annotation exists.
 */
bool dvmIsFieldAnnotationPresent(const ClassObject* clazz,
        const Field* field, const ClassObject* annotationClazz)
{
    const DexAnnotationSetItem* pAnnoSet = findAnnotationSetForField(field);
    if (pAnnoSet == NULL) {
        return NULL;
    }
    const DexAnnotationItem* pAnnoItem = getAnnotationItemFromAnnotationSet(
            clazz, pAnnoSet, kDexVisibilityRuntime, annotationClazz);
    return (pAnnoItem != NULL);
}

/*
 * Retrieve the Signature annotation, if any.  Returns NULL if no signature
 * exists.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
ArrayObject* dvmGetFieldSignatureAnnotation(const Field* field)
{
    ClassObject* clazz = field->clazz;
    const DexAnnotationSetItem* pAnnoSet;
    ArrayObject* signature = NULL;

    pAnnoSet = findAnnotationSetForField(field);
    if (pAnnoSet != NULL)
        signature = getSignatureValue(clazz, pAnnoSet);

    return signature;
}


/*
 * ===========================================================================
 *      Parameter
 * ===========================================================================
 */

/*
 * We have an annotation_set_ref_list, which is essentially a list of
 * entries that we pass to processAnnotationSet().
 *
 * The returned object must be released with dvmReleaseTrackedAlloc.
 */
static ArrayObject* processAnnotationSetRefList(const ClassObject* clazz,
    const DexAnnotationSetRefList* pAnnoSetList, u4 count)
{
    DexFile* pDexFile = clazz->pDvmDex->pDexFile;
    ArrayObject* annoArrayArray = NULL;
    u4 idx;

    /* allocate an array of Annotation arrays to hold results */
    annoArrayArray = dvmAllocArrayByClass(
        gDvm.classJavaLangAnnotationAnnotationArrayArray, count, ALLOC_DEFAULT);
    if (annoArrayArray == NULL) {
        ALOGW("annotation set ref array alloc failed");
        goto bail;
    }

    for (idx = 0; idx < count; idx++) {
        Thread* self = dvmThreadSelf();
        const DexAnnotationSetRefItem* pItem;
        const DexAnnotationSetItem* pAnnoSet;
        Object *annoSet;
        DexAnnotationSetItem emptySet;
        emptySet.size = 0;

        pItem = dexGetParameterAnnotationSetRef(pAnnoSetList, idx);
        pAnnoSet = dexGetSetRefItemItem(pDexFile, pItem);
        if (pAnnoSet == NULL) {
            pAnnoSet = &emptySet;
        }

        annoSet = (Object *)processAnnotationSet(clazz,
                                                 pAnnoSet,
                                                 kDexVisibilityRuntime);
        if (annoSet == NULL) {
            ALOGW("processAnnotationSet failed");
            annoArrayArray = NULL;
            goto bail;
        }
        dvmSetObjectArrayElement(annoArrayArray, idx, annoSet);
        dvmReleaseTrackedAlloc((Object*) annoSet, self);
    }

bail:
    return annoArrayArray;
}

/*
 * Find the DexAnnotationSetItem for this parameter.
 *
 * Returns NULL if none found.
 */
static const DexParameterAnnotationsItem* findAnnotationsItemForMethod(
    const Method* method)
{
    ClassObject* clazz = method->clazz;
    DexFile* pDexFile;
    const DexAnnotationsDirectoryItem* pAnnoDir;
    const DexParameterAnnotationsItem* pParameterList;

    if (clazz->pDvmDex == NULL)         /* generated class (Proxy, array) */
        return NULL;

    pDexFile = clazz->pDvmDex->pDexFile;
    pAnnoDir = getAnnoDirectory(pDexFile, clazz);
    if (pAnnoDir == NULL)
        return NULL;

    pParameterList = dexGetParameterAnnotations(pDexFile, pAnnoDir);
    if (pParameterList == NULL)
        return NULL;

    /*
     * Run through the list and find a matching method.  We compare the
     * method ref indices in the annotation list with the method's DEX
     * method_idx value.
     *
     * TODO: use a binary search for long lists
     *
     * Alternate approach: for each entry in the annotations list,
     * find the method definition in the DEX file and perform string
     * comparisons on class name, method name, and signature.
     */
    u4 methodIdx = dvmGetMethodIdx(method);
    u4 count = dexGetParameterAnnotationsSize(pDexFile, pAnnoDir);
    u4 idx;

    for (idx = 0; idx < count; idx++) {
        if (pParameterList[idx].methodIdx == methodIdx) {
            /* found! */
            return &pParameterList[idx];
        }
    }

    return NULL;
}


/*
 * Count up the number of arguments the method takes.  The "this" pointer
 * doesn't count.
 */
static int countMethodArguments(const Method* method)
{
    /* method->shorty[0] is the return type */
    return strlen(method->shorty + 1);
}

/*
 * Return an array of arrays of Annotation objects.  The outer array has
 * one entry per method parameter, the inner array has the list of annotations
 * associated with that parameter.
 *
 * If the method has no parameters, we return an array of length zero.  If
 * the method has one or more parameters, we return an array whose length
 * is equal to the number of parameters; if a given parameter does not have
 * an annotation, the corresponding entry will be null.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
ArrayObject* dvmGetParameterAnnotations(const Method* method)
{
    ClassObject* clazz = method->clazz;
    const DexParameterAnnotationsItem* pItem;
    ArrayObject* annoArrayArray = NULL;

    pItem = findAnnotationsItemForMethod(method);
    if (pItem != NULL) {
        DexFile* pDexFile = clazz->pDvmDex->pDexFile;
        const DexAnnotationSetRefList* pAnnoSetList;
        u4 size;

        size = dexGetParameterAnnotationSetRefSize(pDexFile, pItem);
        pAnnoSetList = dexGetParameterAnnotationSetRefList(pDexFile, pItem);
        annoArrayArray = processAnnotationSetRefList(clazz, pAnnoSetList, size);
    } else {
        /* no matching annotations found */
        annoArrayArray = emptyAnnoArrayArray(countMethodArguments(method));
    }

    return annoArrayArray;
}


/*
 * ===========================================================================
 *      DexEncodedArray interpretation
 * ===========================================================================
 */

/**
 * Initializes an encoded array iterator.
 *
 * @param iterator iterator to initialize
 * @param encodedArray encoded array to iterate over
 * @param clazz class to use when resolving strings and types
 */
void dvmEncodedArrayIteratorInitialize(EncodedArrayIterator* iterator,
        const DexEncodedArray* encodedArray, const ClassObject* clazz) {
    iterator->encodedArray = encodedArray;
    iterator->cursor = encodedArray->array;
    iterator->size = readUleb128(&iterator->cursor);
    iterator->elementsLeft = iterator->size;
    iterator->clazz = clazz;
}

/**
 * Returns whether there are more elements to be read.
 */
bool dvmEncodedArrayIteratorHasNext(const EncodedArrayIterator* iterator) {
    return (iterator->elementsLeft != 0);
}

/**
 * Returns the next decoded value from the iterator, advancing its
 * cursor. This returns primitive values in their corresponding union
 * slots, and returns everything else (including nulls) as object
 * references in the "l" union slot.
 *
 * The caller must call dvmReleaseTrackedAlloc() on any returned reference.
 *
 * @param value pointer to store decoded value into
 * @returns true if a value was decoded and the cursor advanced; false if
 * the last value had already been decoded or if there was a problem decoding
 */
bool dvmEncodedArrayIteratorGetNext(EncodedArrayIterator* iterator,
        AnnotationValue* value) {
    bool processed;

    if (iterator->elementsLeft == 0) {
        return false;
    }

    processed = processAnnotationValue(iterator->clazz, &iterator->cursor,
            value, kPrimitivesOrObjects);

    if (! processed) {
        ALOGE("Failed to process array element %d from %p",
                iterator->size - iterator->elementsLeft,
                iterator->encodedArray);
        iterator->elementsLeft = 0;
        return false;
    }

    iterator->elementsLeft--;
    return true;
}
