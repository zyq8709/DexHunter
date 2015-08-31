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
 * Basic reflection calls and utility functions.
 */
#include "Dalvik.h"

#include <stdlib.h>

/*
 * For some of the reflection stuff we need to un-box primitives, e.g.
 * convert a java/lang/Integer to int or even a float.  We assume that
 * the first instance field holds the value.
 *
 * To verify this, we either need to ensure that the class has only one
 * instance field, or we need to look up the field by name and verify
 * that it comes first.  The former is simpler, and should work.
 */
bool dvmValidateBoxClasses()
{
    static const char* classes[] = {
        "Ljava/lang/Boolean;",
        "Ljava/lang/Character;",
        "Ljava/lang/Float;",
        "Ljava/lang/Double;",
        "Ljava/lang/Byte;",
        "Ljava/lang/Short;",
        "Ljava/lang/Integer;",
        "Ljava/lang/Long;",
        NULL
    };
    const char** ccp;

    for (ccp = classes; *ccp != NULL; ccp++) {
        ClassObject* clazz;

        clazz = dvmFindClassNoInit(*ccp, NULL);
        if (clazz == NULL) {
            ALOGE("Couldn't find '%s'", *ccp);
            return false;
        }

        if (clazz->ifieldCount != 1) {
            ALOGE("Found %d instance fields in '%s'",
                clazz->ifieldCount, *ccp);
            return false;
        }
    }

    return true;
}


/*
 * Find the named class object.  We have to trim "*pSignature" down to just
 * the first token, do the lookup, and then restore anything important
 * that we've stomped on.
 *
 * "pSig" will be advanced to the start of the next token.
 */
static ClassObject* convertSignaturePartToClass(char** pSignature,
    const ClassObject* defClass)
{
    ClassObject* clazz = NULL;
    char* signature = *pSignature;

    if (*signature == '[') {
        /* looks like "[[[Landroid/debug/Stuff;"; we want the whole thing */
        char savedChar;

        while (*++signature == '[')
            ;
        if (*signature == 'L') {
            while (*++signature != ';')
                ;
        }

        /* advance past ';', and stomp on whatever comes next */
        savedChar = *++signature;
        *signature = '\0';
        clazz = dvmFindArrayClass(*pSignature, defClass->classLoader);
        *signature = savedChar;
    } else if (*signature == 'L') {
        /* looks like 'Landroid/debug/Stuff;"; we want the whole thing */
        char savedChar;
        while (*++signature != ';')
            ;
        savedChar = *++signature;
        *signature = '\0';
        clazz = dvmFindClassNoInit(*pSignature, defClass->classLoader);
        *signature = savedChar;
    } else {
        clazz = dvmFindPrimitiveClass(*signature++);
    }

    if (clazz == NULL) {
        ALOGW("Unable to match class for part: '%s'", *pSignature);
    }
    *pSignature = signature;
    return clazz;
}

/*
 * Convert the method signature to an array of classes.
 *
 * The tokenization process may mangle "*pSignature".  On return, it will
 * be pointing at the closing ')'.
 *
 * "defClass" is the method's class, which is needed to make class loaders
 * happy.
 */
static ArrayObject* convertSignatureToClassArray(char** pSignature,
    ClassObject* defClass)
{
    char* signature = *pSignature;

    assert(*signature == '(');
    signature++;

    /* count up the number of parameters */
    size_t count = 0;
    char* cp = signature;
    while (*cp != ')') {
        count++;

        if (*cp == '[') {
            while (*++cp == '[')
                ;
        }
        if (*cp == 'L') {
            while (*++cp != ';')
                ;
        }
        cp++;
    }
    LOGVV("REFLECT found %d parameters in '%s'", count, *pSignature);

    /* create an array to hold them */
    ArrayObject* classArray = dvmAllocArrayByClass(gDvm.classJavaLangClassArray,
                     count, ALLOC_DEFAULT);
    if (classArray == NULL)
        return NULL;

    /* fill it in */
    cp = signature;
    for (size_t i = 0; i < count; i++) {
        ClassObject* clazz = convertSignaturePartToClass(&cp, defClass);
        if (clazz == NULL) {
            assert(dvmCheckException(dvmThreadSelf()));
            return NULL;
        }
        LOGVV("REFLECT  %d: '%s'", i, clazz->descriptor);
        dvmSetObjectArrayElement(classArray, i, (Object *)clazz);
    }

    *pSignature = cp;

    /* caller must call dvmReleaseTrackedAlloc */
    return classArray;
}


/*
 * Convert a field pointer to a slot number.
 *
 * We use positive values starting from 0 for instance fields, negative
 * values starting from -1 for static fields.
 */
static int fieldToSlot(const Field* field, const ClassObject* clazz)
{
    int slot;

    if (dvmIsStaticField(field)) {
        slot = (StaticField*)field - &clazz->sfields[0];
        assert(slot >= 0 && slot < clazz->sfieldCount);
        slot = -(slot+1);
    } else {
        slot = (InstField*)field - clazz->ifields;
        assert(slot >= 0 && slot < clazz->ifieldCount);
    }

    return slot;
}

/*
 * Convert a slot number to a field pointer.
 */
Field* dvmSlotToField(ClassObject* clazz, int slot)
{
    if (slot < 0) {
        slot = -(slot+1);
        assert(slot < clazz->sfieldCount);
        return (Field*)(void*)&clazz->sfields[slot];
    } else {
        assert(slot < clazz->ifieldCount);
        return (Field*)(void*)&clazz->ifields[slot];
    }
}

/*
 * Create a new java.lang.reflect.Field object from "field".
 *
 * The Field spec doesn't specify the constructor.  We're going to use the
 * one from our existing class libs:
 *
 *  private Field(Class declaringClass, Class type, String name, int slot)
 */
static Object* createFieldObject(Field* field, const ClassObject* clazz)
{
    Object* result = NULL;
    Object* fieldObj = NULL;
    StringObject* nameObj = NULL;
    ClassObject* type;
    char* mangle;
    char* cp;
    int slot, field_idx;

    assert(dvmIsClassInitialized(gDvm.classJavaLangReflectField));

    fieldObj = dvmAllocObject(gDvm.classJavaLangReflectField, ALLOC_DEFAULT);
    if (fieldObj == NULL)
        goto bail;

    cp = mangle = strdup(field->signature);
    type = convertSignaturePartToClass(&cp, clazz);
    free(mangle);
    if (type == NULL)
        goto bail;

    nameObj = dvmCreateStringFromCstr(field->name);
    if (nameObj == NULL)
        goto bail;

    slot = fieldToSlot(field, clazz);
    field_idx = dvmGetFieldIdx(field);

    JValue unused;
    dvmCallMethod(dvmThreadSelf(), gDvm.methJavaLangReflectField_init,
        fieldObj, &unused, clazz, type, nameObj, slot, field_idx);
    if (dvmCheckException(dvmThreadSelf())) {
        ALOGD("Field class init threw exception");
        goto bail;
    }

    result = fieldObj;

bail:
    dvmReleaseTrackedAlloc((Object*) nameObj, NULL);
    if (result == NULL)
        dvmReleaseTrackedAlloc((Object*) fieldObj, NULL);
    /* caller must dvmReleaseTrackedAlloc(result) */
    return result;
}

/*
 *
 * Get an array with all fields declared by a class.
 *
 * This includes both static and instance fields.
 */
ArrayObject* dvmGetDeclaredFields(ClassObject* clazz, bool publicOnly)
{
    if (!dvmIsClassInitialized(gDvm.classJavaLangReflectField))
        dvmInitClass(gDvm.classJavaLangReflectField);

    /* count #of fields */
    size_t count;
    if (!publicOnly)
        count = clazz->sfieldCount + clazz->ifieldCount;
    else {
        count = 0;
        for (int i = 0; i < clazz->sfieldCount; i++) {
            if ((clazz->sfields[i].accessFlags & ACC_PUBLIC) != 0)
                count++;
        }
        for (int i = 0; i < clazz->ifieldCount; i++) {
            if ((clazz->ifields[i].accessFlags & ACC_PUBLIC) != 0)
                count++;
        }
    }

    /* create the Field[] array */
    ArrayObject* fieldArray =
        dvmAllocArrayByClass(gDvm.classJavaLangReflectFieldArray, count, ALLOC_DEFAULT);
    if (fieldArray == NULL)
        return NULL;

    /* populate */
    size_t fieldCount = 0;
    for (int i = 0; i < clazz->sfieldCount; i++) {
        if (!publicOnly ||
            (clazz->sfields[i].accessFlags & ACC_PUBLIC) != 0)
        {
            Object* field = createFieldObject(&clazz->sfields[i], clazz);
            if (field == NULL) {
                goto fail;
            }
            dvmSetObjectArrayElement(fieldArray, fieldCount, field);
            dvmReleaseTrackedAlloc(field, NULL);
            ++fieldCount;
        }
    }
    for (int i = 0; i < clazz->ifieldCount; i++) {
        if (!publicOnly ||
            (clazz->ifields[i].accessFlags & ACC_PUBLIC) != 0)
        {
            Object* field = createFieldObject(&clazz->ifields[i], clazz);
            if (field == NULL) {
                goto fail;
            }
            dvmSetObjectArrayElement(fieldArray, fieldCount, field);
            dvmReleaseTrackedAlloc(field, NULL);
            ++fieldCount;
        }
    }

    assert(fieldCount == fieldArray->length);

    /* caller must call dvmReleaseTrackedAlloc */
    return fieldArray;

fail:
    dvmReleaseTrackedAlloc((Object*) fieldArray, NULL);
    return NULL;
}


/*
 * Convert a method pointer to a slot number.
 *
 * We use positive values starting from 0 for virtual methods, negative
 * values starting from -1 for static methods.
 */
static int methodToSlot(const Method* meth)
{
    ClassObject* clazz = meth->clazz;
    int slot;

    if (dvmIsDirectMethod(meth)) {
        slot = meth - clazz->directMethods;
        assert(slot >= 0 && slot < clazz->directMethodCount);
        slot = -(slot+1);
    } else {
        slot = meth - clazz->virtualMethods;
        assert(slot >= 0 && slot < clazz->virtualMethodCount);
    }

    return slot;
}

/*
 * Convert a slot number to a method pointer.
 */
Method* dvmSlotToMethod(ClassObject* clazz, int slot)
{
    if (slot < 0) {
        slot = -(slot+1);
        assert(slot < clazz->directMethodCount);
        return &clazz->directMethods[slot];
    } else {
        assert(slot < clazz->virtualMethodCount);
        return &clazz->virtualMethods[slot];
    }
}

/*
 * Create a new java/lang/reflect/Constructor object, using the contents of
 * "meth" to construct it.
 *
 * The spec doesn't specify the constructor.  We're going to use the
 * one from our existing class libs:
 *
 *  private Constructor (Class declaringClass, Class[] ptypes, Class[] extypes,
 *      int slot)
 */
static Object* createConstructorObject(Method* meth)
{
    Object* result = NULL;
    ArrayObject* params = NULL;
    ArrayObject* exceptions = NULL;
    Object* consObj;
    DexStringCache mangle;
    char* cp;
    int slot, method_idx;

    dexStringCacheInit(&mangle);

    /* parent should guarantee init so we don't have to check on every call */
    assert(dvmIsClassInitialized(gDvm.classJavaLangReflectConstructor));

    consObj = dvmAllocObject(gDvm.classJavaLangReflectConstructor,
                ALLOC_DEFAULT);
    if (consObj == NULL)
        goto bail;

    /*
     * Convert the signature string into an array of classes representing
     * the arguments.
     */
    cp = dvmCopyDescriptorStringFromMethod(meth, &mangle);
    params = convertSignatureToClassArray(&cp, meth->clazz);
    if (params == NULL)
        goto bail;
    assert(*cp == ')');
    assert(*(cp+1) == 'V');

    /*
     * Create an array with one entry for every exception that the class
     * is declared to throw.
     */
    exceptions = dvmGetMethodThrows(meth);
    if (dvmCheckException(dvmThreadSelf()))
        goto bail;

    slot = methodToSlot(meth);
    method_idx = dvmGetMethodIdx(meth);

    JValue unused;
    dvmCallMethod(dvmThreadSelf(), gDvm.methJavaLangReflectConstructor_init,
        consObj, &unused, meth->clazz, params, exceptions, slot, method_idx);
    if (dvmCheckException(dvmThreadSelf())) {
        ALOGD("Constructor class init threw exception");
        goto bail;
    }

    result = consObj;

bail:
    dexStringCacheRelease(&mangle);
    dvmReleaseTrackedAlloc((Object*) params, NULL);
    dvmReleaseTrackedAlloc((Object*) exceptions, NULL);
    if (result == NULL) {
        assert(dvmCheckException(dvmThreadSelf()));
        dvmReleaseTrackedAlloc(consObj, NULL);
    }
    /* caller must dvmReleaseTrackedAlloc(result) */
    return result;
}

/*
 * Get an array with all constructors declared by a class.
 */
ArrayObject* dvmGetDeclaredConstructors(ClassObject* clazz, bool publicOnly)
{
    if (!dvmIsClassInitialized(gDvm.classJavaLangReflectConstructor))
        dvmInitClass(gDvm.classJavaLangReflectConstructor);

    /*
     * Ordinarily we init the class the first time we resolve a method.
     * We're bypassing the normal resolution mechanism, so we init it here.
     */
    if (!dvmIsClassInitialized(clazz))
        dvmInitClass(clazz);

    /*
     * Count up the #of relevant methods.
     */
    size_t count = 0;
    for (int i = 0; i < clazz->directMethodCount; ++i) {
        Method* meth = &clazz->directMethods[i];
        if ((!publicOnly || dvmIsPublicMethod(meth)) &&
            dvmIsConstructorMethod(meth) && !dvmIsStaticMethod(meth))
        {
            count++;
        }
    }

    /*
     * Create an array of Constructor objects.
     */
    ClassObject* arrayClass = gDvm.classJavaLangReflectConstructorArray;
    ArrayObject* ctorArray = dvmAllocArrayByClass(arrayClass, count, ALLOC_DEFAULT);
    if (ctorArray == NULL)
        return NULL;

    /*
     * Fill out the array.
     */
    size_t ctorObjCount = 0;
    for (int i = 0; i < clazz->directMethodCount; ++i) {
        Method* meth = &clazz->directMethods[i];
        if ((!publicOnly || dvmIsPublicMethod(meth)) &&
            dvmIsConstructorMethod(meth) && !dvmIsStaticMethod(meth))
        {
            Object* ctorObj = createConstructorObject(meth);
            if (ctorObj == NULL) {
              dvmReleaseTrackedAlloc((Object*) ctorArray, NULL);
              return NULL;
            }
            dvmSetObjectArrayElement(ctorArray, ctorObjCount, ctorObj);
            ++ctorObjCount;
            dvmReleaseTrackedAlloc(ctorObj, NULL);
        }
    }

    assert(ctorObjCount == ctorArray->length);

    /* caller must call dvmReleaseTrackedAlloc */
    return ctorArray;
}

/*
 * Create a new java/lang/reflect/Method object, using the contents of
 * "meth" to construct it.
 *
 * The spec doesn't specify the constructor.  We're going to use the
 * one from our existing class libs:
 *
 *  private Method(Class declaring, Class[] paramTypes, Class[] exceptTypes,
 *      Class returnType, String name, int slot)
 *
 * The caller must call dvmReleaseTrackedAlloc() on the result.
 */
Object* dvmCreateReflectMethodObject(const Method* meth)
{
    Object* result = NULL;
    ArrayObject* params = NULL;
    ArrayObject* exceptions = NULL;
    StringObject* nameObj = NULL;
    Object* methObj;
    ClassObject* returnType;
    DexStringCache mangle;
    char* cp;
    int slot, method_idx;

    if (dvmCheckException(dvmThreadSelf())) {
        ALOGW("WARNING: dvmCreateReflectMethodObject called with "
             "exception pending");
        return NULL;
    }

    dexStringCacheInit(&mangle);

    /* parent should guarantee init so we don't have to check on every call */
    assert(dvmIsClassInitialized(gDvm.classJavaLangReflectMethod));

    methObj = dvmAllocObject(gDvm.classJavaLangReflectMethod, ALLOC_DEFAULT);
    if (methObj == NULL)
        goto bail;

    /*
     * Convert the signature string into an array of classes representing
     * the arguments, and a class for the return type.
     */
    cp = dvmCopyDescriptorStringFromMethod(meth, &mangle);
    params = convertSignatureToClassArray(&cp, meth->clazz);
    if (params == NULL)
        goto bail;
    assert(*cp == ')');
    cp++;
    returnType = convertSignaturePartToClass(&cp, meth->clazz);
    if (returnType == NULL)
        goto bail;

    /*
     * Create an array with one entry for every exception that the class
     * is declared to throw.
     */
    exceptions = dvmGetMethodThrows(meth);
    if (dvmCheckException(dvmThreadSelf()))
        goto bail;

    /* method name */
    nameObj = dvmCreateStringFromCstr(meth->name);
    if (nameObj == NULL)
        goto bail;

    slot = methodToSlot(meth);
    method_idx = dvmGetMethodIdx(meth);

    JValue unused;
    dvmCallMethod(dvmThreadSelf(), gDvm.methJavaLangReflectMethod_init,
        methObj, &unused, meth->clazz, params, exceptions, returnType,
        nameObj, slot, method_idx);
    if (dvmCheckException(dvmThreadSelf())) {
        ALOGD("Method class init threw exception");
        goto bail;
    }

    result = methObj;

bail:
    dexStringCacheRelease(&mangle);
    if (result == NULL) {
        assert(dvmCheckException(dvmThreadSelf()));
    }
    dvmReleaseTrackedAlloc((Object*) nameObj, NULL);
    dvmReleaseTrackedAlloc((Object*) params, NULL);
    dvmReleaseTrackedAlloc((Object*) exceptions, NULL);
    if (result == NULL)
        dvmReleaseTrackedAlloc(methObj, NULL);
    return result;
}

/*
 * Get an array with all methods declared by a class.
 *
 * This includes both static and virtual methods, and can include private
 * members if "publicOnly" is false.  It does not include Miranda methods,
 * since those weren't declared in the class, or constructors.
 */
ArrayObject* dvmGetDeclaredMethods(ClassObject* clazz, bool publicOnly)
{
    if (!dvmIsClassInitialized(gDvm.classJavaLangReflectMethod))
        dvmInitClass(gDvm.classJavaLangReflectMethod);

    /*
     * Count up the #of relevant methods.
     *
     * Ignore virtual Miranda methods and direct class/object constructors.
     */
    size_t count = 0;
    Method* meth = clazz->virtualMethods;
    for (int i = 0; i < clazz->virtualMethodCount; i++, meth++) {
        if ((!publicOnly || dvmIsPublicMethod(meth)) &&
            !dvmIsMirandaMethod(meth))
        {
            count++;
        }
    }
    meth = clazz->directMethods;
    for (int i = 0; i < clazz->directMethodCount; i++, meth++) {
        if ((!publicOnly || dvmIsPublicMethod(meth)) && meth->name[0] != '<') {
            count++;
        }
    }

    /*
     * Create an array of Method objects.
     */
    ArrayObject* methodArray =
        dvmAllocArrayByClass(gDvm.classJavaLangReflectMethodArray, count, ALLOC_DEFAULT);
    if (methodArray == NULL)
        return NULL;

    /*
     * Fill out the array.
     */
    meth = clazz->virtualMethods;
    size_t methObjCount = 0;
    for (int i = 0; i < clazz->virtualMethodCount; i++, meth++) {
        if ((!publicOnly || dvmIsPublicMethod(meth)) &&
            !dvmIsMirandaMethod(meth))
        {
            Object* methObj = dvmCreateReflectMethodObject(meth);
            if (methObj == NULL)
                goto fail;
            dvmSetObjectArrayElement(methodArray, methObjCount, methObj);
            ++methObjCount;
            dvmReleaseTrackedAlloc(methObj, NULL);
        }
    }
    meth = clazz->directMethods;
    for (int i = 0; i < clazz->directMethodCount; i++, meth++) {
        if ((!publicOnly || dvmIsPublicMethod(meth)) &&
            meth->name[0] != '<')
        {
            Object* methObj = dvmCreateReflectMethodObject(meth);
            if (methObj == NULL)
                goto fail;
            dvmSetObjectArrayElement(methodArray, methObjCount, methObj);
            ++methObjCount;
            dvmReleaseTrackedAlloc(methObj, NULL);
        }
    }

    assert(methObjCount == methodArray->length);

    /* caller must call dvmReleaseTrackedAlloc */
    return methodArray;

fail:
    dvmReleaseTrackedAlloc((Object*) methodArray, NULL);
    return NULL;
}

/*
 * Fills targetDescriptorCache with the descriptors of the classes in args.
 * This is the concatenation of the descriptors with no other adornment,
 * consistent with dexProtoGetParameterDescriptors.
 */
static void createTargetDescriptor(ArrayObject* args,
    DexStringCache* targetDescriptorCache)
{
    ClassObject** argsArray = (ClassObject**)(void*)args->contents;
    size_t length = 1; /* +1 for the terminating '\0' */
    for (size_t i = 0; i < args->length; ++i) {
        length += strlen(argsArray[i]->descriptor);
    }

    dexStringCacheAlloc(targetDescriptorCache, length);

    char* at = (char*) targetDescriptorCache->value;
    for (size_t i = 0; i < args->length; ++i) {
        const char* descriptor = argsArray[i]->descriptor;
        strcpy(at, descriptor);
        at += strlen(descriptor);
    }
}

static Object* findConstructorOrMethodInArray(int methodsCount, Method* methods,
    const char* name, const char* parameterDescriptors)
{
    Method* method = NULL;
    Method* result = NULL;
    int i;

    for (i = 0; i < methodsCount; ++i) {
        method = &methods[i];
        if (strcmp(name, method->name) != 0
            || dvmIsMirandaMethod(method)
            || dexProtoCompareToParameterDescriptors(&method->prototype,
                    parameterDescriptors) != 0) {
            continue;
        }

        result = method;

        /*
         * Covariant return types permit the class to define multiple
         * methods with the same name and parameter types. Prefer to return
         * a non-synthetic method in such situations. We may still return
         * a synthetic method to handle situations like escalated visibility.
         */
        if (!dvmIsSyntheticMethod(method)) {
            break;
        }
    }

    if (result != NULL) {
        return dvmCreateReflectObjForMethod(result->clazz, result);
    }

    return NULL;
}

/*
 * Get the named method.
 */
Object* dvmGetDeclaredConstructorOrMethod(ClassObject* clazz,
    StringObject* nameObj, ArrayObject* args)
{
    Object* result = NULL;
    DexStringCache targetDescriptorCache;
    char* name;
    const char* targetDescriptor;

    dexStringCacheInit(&targetDescriptorCache);

    name = dvmCreateCstrFromString(nameObj);
    createTargetDescriptor(args, &targetDescriptorCache);
    targetDescriptor = targetDescriptorCache.value;

    result = findConstructorOrMethodInArray(clazz->directMethodCount,
        clazz->directMethods, name, targetDescriptor);
    if (result == NULL) {
        result = findConstructorOrMethodInArray(clazz->virtualMethodCount,
            clazz->virtualMethods, name, targetDescriptor);
    }

    free(name);
    dexStringCacheRelease(&targetDescriptorCache);
    return result;
}

/*
 * Get the named field.
 */
Object* dvmGetDeclaredField(ClassObject* clazz, StringObject* nameObj)
{
    int i;
    Object* fieldObj = NULL;
    char* name = dvmCreateCstrFromString(nameObj);

    if (!dvmIsClassInitialized(gDvm.classJavaLangReflectField))
        dvmInitClass(gDvm.classJavaLangReflectField);

    for (i = 0; i < clazz->sfieldCount; i++) {
        Field* field = &clazz->sfields[i];
        if (strcmp(name, field->name) == 0) {
            fieldObj = createFieldObject(field, clazz);
            break;
        }
    }
    if (fieldObj == NULL) {
        for (i = 0; i < clazz->ifieldCount; i++) {
            Field* field = &clazz->ifields[i];
            if (strcmp(name, field->name) == 0) {
                fieldObj = createFieldObject(field, clazz);
                break;
            }
        }
    }

    free(name);
    return fieldObj;
}

/*
 * Get all interfaces a class implements. If this is unable to allocate
 * the result array, this raises an OutOfMemoryError and returns NULL.
 */
ArrayObject* dvmGetInterfaces(ClassObject* clazz)
{
    if (!dvmIsClassInitialized(gDvm.classJavaLangReflectMethod))
        dvmInitClass(gDvm.classJavaLangReflectMethod);

    /*
     * Create an array of Class objects.
     */
    size_t count = clazz->interfaceCount;
    ArrayObject* interfaceArray =
        dvmAllocArrayByClass(gDvm.classJavaLangClassArray, count, ALLOC_DEFAULT);
    if (interfaceArray == NULL)
        return NULL;

    /*
     * Fill out the array.
     */
    memcpy(interfaceArray->contents, clazz->interfaces,
           count * sizeof(Object *));
    dvmWriteBarrierArray(interfaceArray, 0, count);

    /* caller must call dvmReleaseTrackedAlloc */
    return interfaceArray;
}

/*
 * Given a boxed primitive type, such as java/lang/Integer, return the
 * primitive type index.
 *
 * Returns PRIM_NOT for void, since we never "box" that.
 */
static PrimitiveType getBoxedType(DataObject* arg)
{
    static const int kJavaLangLen = 11;     // strlen("Ljava/lang/")

    if (arg == NULL)
        return PRIM_NOT;

    const char* name = arg->clazz->descriptor;

    if (strncmp(name, "Ljava/lang/", kJavaLangLen) != 0)
        return PRIM_NOT;

    if (strcmp(name + kJavaLangLen, "Boolean;") == 0)
        return PRIM_BOOLEAN;
    if (strcmp(name + kJavaLangLen, "Character;") == 0)
        return PRIM_CHAR;
    if (strcmp(name + kJavaLangLen, "Float;") == 0)
        return PRIM_FLOAT;
    if (strcmp(name + kJavaLangLen, "Double;") == 0)
        return PRIM_DOUBLE;
    if (strcmp(name + kJavaLangLen, "Byte;") == 0)
        return PRIM_BYTE;
    if (strcmp(name + kJavaLangLen, "Short;") == 0)
        return PRIM_SHORT;
    if (strcmp(name + kJavaLangLen, "Integer;") == 0)
        return PRIM_INT;
    if (strcmp(name + kJavaLangLen, "Long;") == 0)
        return PRIM_LONG;
    return PRIM_NOT;
}

/*
 * Convert primitive, boxed data from "srcPtr" to "dstPtr".
 *
 * Section v2 2.6 lists the various conversions and promotions.  We
 * allow the "widening" and "identity" conversions, but don't allow the
 * "narrowing" conversions.
 *
 * Allowed:
 *  byte to short, int, long, float, double
 *  short to int, long, float double
 *  char to int, long, float, double
 *  int to long, float, double
 *  long to float, double
 *  float to double
 * Values of types byte, char, and short are "internally" widened to int.
 *
 * Returns the width in 32-bit words of the destination primitive, or
 * -1 if the conversion is not allowed.
 *
 * TODO? use JValue rather than u4 pointers
 */
int dvmConvertPrimitiveValue(PrimitiveType srcType,
    PrimitiveType dstType, const s4* srcPtr, s4* dstPtr)
{
    enum Conversion {
        OK4, OK8, ItoJ, ItoD, JtoD, FtoD, ItoF, JtoF, bad
    };

    enum Conversion conv;
#ifdef ARCH_HAVE_ALIGNED_DOUBLES
    double ret;
#endif

    assert((srcType != PRIM_VOID) && (srcType != PRIM_NOT));
    assert((dstType != PRIM_VOID) && (dstType != PRIM_NOT));

    switch (dstType) {
        case PRIM_BOOLEAN:
        case PRIM_CHAR:
        case PRIM_BYTE: {
            conv = (srcType == dstType) ? OK4 : bad;
            break;
        }
        case PRIM_SHORT: {
            switch (srcType) {
                case PRIM_BYTE:
                case PRIM_SHORT: conv = OK4; break;
                default:         conv = bad; break;
            }
            break;
        }
        case PRIM_INT: {
            switch (srcType) {
                case PRIM_BYTE:
                case PRIM_CHAR:
                case PRIM_SHORT:
                case PRIM_INT:   conv = OK4; break;
                default:         conv = bad; break;
            }
            break;
        }
        case PRIM_LONG: {
            switch (srcType) {
                case PRIM_BYTE:
                case PRIM_CHAR:
                case PRIM_SHORT:
                case PRIM_INT:   conv = ItoJ; break;
                case PRIM_LONG:  conv = OK8;  break;
                default:         conv = bad;  break;
            }
            break;
        }
        case PRIM_FLOAT: {
            switch (srcType) {
                case PRIM_BYTE:
                case PRIM_CHAR:
                case PRIM_SHORT:
                case PRIM_INT:   conv = ItoF; break;
                case PRIM_LONG:  conv = JtoF; break;
                case PRIM_FLOAT: conv = OK4;  break;
                default:         conv = bad;  break;
            }
            break;
        }
        case PRIM_DOUBLE: {
            switch (srcType) {
                case PRIM_BYTE:
                case PRIM_CHAR:
                case PRIM_SHORT:
                case PRIM_INT:    conv = ItoD; break;
                case PRIM_LONG:   conv = JtoD; break;
                case PRIM_FLOAT:  conv = FtoD; break;
                case PRIM_DOUBLE: conv = OK8;  break;
                default:          conv = bad;  break;
            }
            break;
        }
        case PRIM_VOID:
        case PRIM_NOT:
        default: {
            conv = bad;
            break;
        }
    }

    switch (conv) {
        case OK4:  *dstPtr = *srcPtr;                                   return 1;
        case OK8:  *(s8*) dstPtr = *(s8*)srcPtr;                        return 2;
        case ItoJ: *(s8*) dstPtr = (s8) (*(s4*) srcPtr);                return 2;
#ifndef ARCH_HAVE_ALIGNED_DOUBLES
        case ItoD: *(double*) dstPtr = (double) (*(s4*) srcPtr);        return 2;
        case JtoD: *(double*) dstPtr = (double) (*(long long*) srcPtr); return 2;
        case FtoD: *(double*) dstPtr = (double) (*(float*) srcPtr);     return 2;
#else
        case ItoD: ret = (double) (*(s4*) srcPtr); memcpy(dstPtr, &ret, 8); return 2;
        case JtoD: ret = (double) (*(long long*) srcPtr); memcpy(dstPtr, &ret, 8); return 2;
        case FtoD: ret = (double) (*(float*) srcPtr); memcpy(dstPtr, &ret, 8); return 2;
#endif
        case ItoF: *(float*) dstPtr = (float) (*(int*) srcPtr);         return 1;
        case JtoF: *(float*) dstPtr = (float) (*(long long*) srcPtr);   return 1;
        case bad: {
            ALOGV("illegal primitive conversion: '%s' to '%s'",
                    dexGetPrimitiveTypeDescriptor(srcType),
                    dexGetPrimitiveTypeDescriptor(dstType));
            return -1;
        }
        default: {
            dvmAbort();
            return -1; // Keep the compiler happy.
        }
    }
}

/*
 * Convert types and widen primitives.  Puts the value of "arg" into
 * "destPtr".
 *
 * Returns the width of the argument in 32-bit words (1 or 2), or -1 on error.
 */
int dvmConvertArgument(DataObject* arg, ClassObject* type, s4* destPtr)
{
    int retVal;

    if (dvmIsPrimitiveClass(type)) {
        /* e.g.: "arg" is java/lang/Float instance, "type" is VM float class */
        PrimitiveType srcType;
        s4* valuePtr;

        srcType = getBoxedType(arg);
        if (srcType == PRIM_NOT) {     // didn't pass a boxed primitive in
            LOGVV("conv arg: type '%s' not boxed primitive",
                arg->clazz->descriptor);
            return -1;
        }

        /* assumes value is stored in first instance field */
        valuePtr = (s4*) arg->instanceData;

        retVal = dvmConvertPrimitiveValue(srcType, type->primitiveType,
                    valuePtr, destPtr);
    } else {
        /* verify object is compatible */
        if ((arg == NULL) || dvmInstanceof(arg->clazz, type)) {
            *destPtr = (s4) arg;
            retVal = 1;
        } else {
            LOGVV("Arg %p (%s) not compatible with %s",
                arg, arg->clazz->descriptor, type->descriptor);
            retVal = -1;
        }
    }

    return retVal;
}

/*
 * Create a wrapper object for a primitive data type.  If "returnType" is
 * not primitive, this just casts "value" to an object and returns it.
 *
 * We could invoke the "toValue" method on the box types to take
 * advantage of pre-created values, but running that through the
 * interpreter is probably less efficient than just allocating storage here.
 *
 * The caller must call dvmReleaseTrackedAlloc on the result.
 */
DataObject* dvmBoxPrimitive(JValue value, ClassObject* returnType)
{
    ClassObject* wrapperClass;
    DataObject* wrapperObj;
    s4* dataPtr;
    PrimitiveType typeIndex = returnType->primitiveType;
    const char* classDescriptor;

    if (typeIndex == PRIM_NOT) {
        /* add to tracking table so return value is always in table */
        if (value.l != NULL)
            dvmAddTrackedAlloc((Object*)value.l, NULL);
        return (DataObject*) value.l;
    }

    classDescriptor = dexGetBoxedTypeDescriptor(typeIndex);
    if (classDescriptor == NULL) {
        return NULL;
    }

    wrapperClass = dvmFindSystemClass(classDescriptor);
    if (wrapperClass == NULL) {
        ALOGW("Unable to find '%s'", classDescriptor);
        assert(dvmCheckException(dvmThreadSelf()));
        return NULL;
    }

    wrapperObj = (DataObject*) dvmAllocObject(wrapperClass, ALLOC_DEFAULT);
    if (wrapperObj == NULL)
        return NULL;
    dataPtr = (s4*) wrapperObj->instanceData;

    /* assumes value is stored in first instance field */
    /* (see dvmValidateBoxClasses) */
    if (typeIndex == PRIM_LONG || typeIndex == PRIM_DOUBLE)
        *(s8*)dataPtr = value.j;
    else
        *dataPtr = value.i;

    return wrapperObj;
}

/*
 * Unwrap a primitive data type, if necessary.
 *
 * If "returnType" is not primitive, we just tuck "value" into JValue and
 * return it after verifying that it's the right type of object.
 *
 * Fails if the field is primitive and "value" is either not a boxed
 * primitive or is of a type that cannot be converted.
 *
 * Returns "true" on success, "false" on failure.
 */
bool dvmUnboxPrimitive(Object* value, ClassObject* returnType,
    JValue* pResult)
{
    PrimitiveType typeIndex = returnType->primitiveType;
    PrimitiveType valueIndex;

    if (typeIndex == PRIM_NOT) {
        if (value != NULL && !dvmInstanceof(value->clazz, returnType)) {
            ALOGD("wrong object type: %s %s",
                value->clazz->descriptor, returnType->descriptor);
            return false;
        }
        pResult->l = value;
        return true;
    } else if (typeIndex == PRIM_VOID) {
        /* can't put anything into a void */
        return false;
    }

    valueIndex = getBoxedType((DataObject*)value);
    if (valueIndex == PRIM_NOT)
        return false;

    /* assumes value is stored in first instance field of "value" */
    /* (see dvmValidateBoxClasses) */
    if (dvmConvertPrimitiveValue(valueIndex, typeIndex,
            (s4*) ((DataObject*)value)->instanceData, (s4*)pResult) < 0)
    {
        ALOGV("Prim conversion failed");
        return false;
    }

    return true;
}


/*
 * Find the return type in the signature, and convert it to a class
 * object.  For primitive types we use a boxed class, for reference types
 * we do a name lookup.
 *
 * On failure, we return NULL with an exception raised.
 */
ClassObject* dvmGetBoxedReturnType(const Method* meth)
{
    const char* sig = dexProtoGetReturnType(&meth->prototype);

    switch (*sig) {
    case 'Z':
    case 'C':
    case 'F':
    case 'D':
    case 'B':
    case 'S':
    case 'I':
    case 'J':
    case 'V':
        return dvmFindPrimitiveClass(*sig);
    case '[':
    case 'L':
        return dvmFindClass(sig, meth->clazz->classLoader);
    default: {
        /* should not have passed verification */
        char* desc = dexProtoCopyMethodDescriptor(&meth->prototype);
        ALOGE("Bad return type in signature '%s'", desc);
        free(desc);
        dvmThrowInternalError(NULL);
        return NULL;
    }
    }
}


/*
 * JNI reflection support: convert reflection object to Field ptr.
 */
Field* dvmGetFieldFromReflectObj(Object* obj)
{
    ClassObject* clazz;
    int slot;

    assert(obj->clazz == gDvm.classJavaLangReflectField);
    clazz = (ClassObject*)dvmGetFieldObject(obj,
                                gDvm.offJavaLangReflectField_declClass);
    slot = dvmGetFieldInt(obj, gDvm.offJavaLangReflectField_slot);

    /* must initialize the class before returning a field ID */
    if (!dvmInitClass(clazz))
        return NULL;

    return dvmSlotToField(clazz, slot);
}

/*
 * JNI reflection support: convert reflection object to Method ptr.
 */
Method* dvmGetMethodFromReflectObj(Object* obj)
{
    ClassObject* clazz;
    int slot;

    if (obj->clazz == gDvm.classJavaLangReflectConstructor) {
        clazz = (ClassObject*)dvmGetFieldObject(obj,
                                gDvm.offJavaLangReflectConstructor_declClass);
        slot = dvmGetFieldInt(obj, gDvm.offJavaLangReflectConstructor_slot);
    } else if (obj->clazz == gDvm.classJavaLangReflectMethod) {
        clazz = (ClassObject*)dvmGetFieldObject(obj,
                                gDvm.offJavaLangReflectMethod_declClass);
        slot = dvmGetFieldInt(obj, gDvm.offJavaLangReflectMethod_slot);
    } else {
        assert(false);
        return NULL;
    }

    /* must initialize the class before returning a method ID */
    if (!dvmInitClass(clazz))
        return NULL;

    return dvmSlotToMethod(clazz, slot);
}

/*
 * JNI reflection support: convert Field to reflection object.
 *
 * The return value is a java.lang.reflect.Field.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
Object* dvmCreateReflectObjForField(const ClassObject* clazz, Field* field)
{
    if (!dvmIsClassInitialized(gDvm.classJavaLangReflectField))
        dvmInitClass(gDvm.classJavaLangReflectField);

    /* caller must dvmReleaseTrackedAlloc(result) */
    return createFieldObject(field, clazz);
}

/*
 * JNI reflection support: convert Method to reflection object.
 *
 * The returned object will be either a java.lang.reflect.Method or
 * .Constructor, depending on whether "method" is a constructor.
 *
 * This is also used for certain "system" annotations.
 *
 * Caller must call dvmReleaseTrackedAlloc().
 */
Object* dvmCreateReflectObjForMethod(const ClassObject* clazz, Method* method)
{
    UNUSED_PARAMETER(clazz);

    if (strcmp(method->name, "<init>") == 0) {
        if (!dvmIsClassInitialized(gDvm.classJavaLangReflectConstructor))
            dvmInitClass(gDvm.classJavaLangReflectConstructor);

        return createConstructorObject(method);
    } else {
        if (!dvmIsClassInitialized(gDvm.classJavaLangReflectMethod))
            dvmInitClass(gDvm.classJavaLangReflectMethod);

        return dvmCreateReflectMethodObject(method);
    }
}
