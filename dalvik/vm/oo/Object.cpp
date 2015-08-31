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
 * Operations on an Object.
 */
#include "Dalvik.h"

/*
 * Find a matching field, in the current class only.
 *
 * Returns NULL if the field can't be found.  (Does not throw an exception.)
 */
InstField* dvmFindInstanceField(const ClassObject* clazz,
    const char* fieldName, const char* signature)
{
    InstField* pField;
    int i;

    assert(clazz != NULL);

    /*
     * Find a field with a matching name and signature.  The Java programming
     * language does not allow you to have two fields with the same name
     * and different types, but the Java VM spec does allow it, so we can't
     * bail out early when the name matches.
     */
    pField = clazz->ifields;
    for (i = 0; i < clazz->ifieldCount; i++, pField++) {
        if (strcmp(fieldName, pField->name) == 0 &&
            strcmp(signature, pField->signature) == 0)
        {
            return pField;
        }
    }

    return NULL;
}

/*
 * Find a matching field, in this class or a superclass.
 *
 * Searching through interfaces isn't necessary, because interface fields
 * are inherently public/static/final.
 *
 * Returns NULL if the field can't be found.  (Does not throw an exception.)
 */
InstField* dvmFindInstanceFieldHier(const ClassObject* clazz,
    const char* fieldName, const char* signature)
{
    InstField* pField;

    /*
     * Search for a match in the current class.
     */
    pField = dvmFindInstanceField(clazz, fieldName, signature);
    if (pField != NULL)
        return pField;

    if (clazz->super != NULL)
        return dvmFindInstanceFieldHier(clazz->super, fieldName, signature);
    else
        return NULL;
}


/*
 * Find a matching field, in this class or an interface.
 *
 * Returns NULL if the field can't be found.  (Does not throw an exception.)
 */
StaticField* dvmFindStaticField(const ClassObject* clazz,
    const char* fieldName, const char* signature)
{
    const StaticField* pField;
    int i;

    assert(clazz != NULL);

    /*
     * Find a field with a matching name and signature.  As with instance
     * fields, the VM allows you to have two fields with the same name so
     * long as they have different types.
     */
    pField = &clazz->sfields[0];
    for (i = 0; i < clazz->sfieldCount; i++, pField++) {
        if (strcmp(fieldName, pField->name) == 0 &&
            strcmp(signature, pField->signature) == 0)
        {
            return (StaticField*) pField;
        }
    }

    return NULL;
}

/*
 * Find a matching field, in this class or a superclass.
 *
 * Returns NULL if the field can't be found.  (Does not throw an exception.)
 */
StaticField* dvmFindStaticFieldHier(const ClassObject* clazz,
    const char* fieldName, const char* signature)
{
    StaticField* pField;

    /*
     * Search for a match in the current class.
     */
    pField = dvmFindStaticField(clazz, fieldName, signature);
    if (pField != NULL)
        return pField;

    /*
     * See if it's in any of our interfaces.  We don't check interfaces
     * inherited from the superclass yet.
     *
     * (Note the set may have been stripped down because of redundancy with
     * the superclass; see notes in createIftable.)
     */
    int i = 0;
    if (clazz->super != NULL) {
        assert(clazz->iftableCount >= clazz->super->iftableCount);
        i = clazz->super->iftableCount;
    }
    for ( ; i < clazz->iftableCount; i++) {
        ClassObject* iface = clazz->iftable[i].clazz;
        pField = dvmFindStaticField(iface, fieldName, signature);
        if (pField != NULL)
            return pField;
    }

    if (clazz->super != NULL)
        return dvmFindStaticFieldHier(clazz->super, fieldName, signature);
    else
        return NULL;
}

/*
 * Find a matching field, in this class or a superclass.
 *
 * We scan both the static and instance field lists in the class.  If it's
 * not found there, we check the direct interfaces, and then recursively
 * scan the superclasses.  This is the order prescribed in the VM spec
 * (v2 5.4.3.2).
 *
 * In most cases we know that we're looking for either a static or an
 * instance field and there's no value in searching through both types.
 * During verification we need to recognize and reject certain unusual
 * situations, and we won't see them unless we walk the lists this way.
 */
Field* dvmFindFieldHier(const ClassObject* clazz, const char* fieldName,
    const char* signature)
{
    Field* pField;

    /*
     * Search for a match in the current class.  Which set we scan first
     * doesn't really matter.
     */
    pField = (Field*) dvmFindStaticField(clazz, fieldName, signature);
    if (pField != NULL)
        return pField;
    pField = (Field*) dvmFindInstanceField(clazz, fieldName, signature);
    if (pField != NULL)
        return pField;

    /*
     * See if it's in any of our interfaces.  We don't check interfaces
     * inherited from the superclass yet.
     */
    int i = 0;
    if (clazz->super != NULL) {
        assert(clazz->iftableCount >= clazz->super->iftableCount);
        i = clazz->super->iftableCount;
    }
    for ( ; i < clazz->iftableCount; i++) {
        ClassObject* iface = clazz->iftable[i].clazz;
        pField = (Field*) dvmFindStaticField(iface, fieldName, signature);
        if (pField != NULL)
            return pField;
    }

    if (clazz->super != NULL)
        return dvmFindFieldHier(clazz->super, fieldName, signature);
    else
        return NULL;
}


/*
 * Compare the given name, return type, and argument types with the contents
 * of the given method. This returns 0 if they are equal and non-zero if not.
 */
static inline int compareMethodHelper(Method* method, const char* methodName,
    const char* returnType, size_t argCount, const char** argTypes)
{
    DexParameterIterator iterator;
    const DexProto* proto;

    if (strcmp(methodName, method->name) != 0) {
        return 1;
    }

    proto = &method->prototype;

    if (strcmp(returnType, dexProtoGetReturnType(proto)) != 0) {
        return 1;
    }

    if (dexProtoGetParameterCount(proto) != argCount) {
        return 1;
    }

    dexParameterIteratorInit(&iterator, proto);

    for (/*argCount*/; argCount != 0; argCount--, argTypes++) {
        const char* argType = *argTypes;
        const char* paramType = dexParameterIteratorNextDescriptor(&iterator);

        if (paramType == NULL) {
            /* Param list ended early; no match */
            break;
        } else if (strcmp(argType, paramType) != 0) {
            /* Types aren't the same; no match. */
            break;
        }
    }

    if (argCount == 0) {
        /* We ran through all the given arguments... */
        if (dexParameterIteratorNextDescriptor(&iterator) == NULL) {
            /* ...and through all the method's arguments; success! */
            return 0;
        }
    }

    return 1;
}

/*
 * Get the count of arguments in the given method descriptor string,
 * and also find a pointer to the return type.
 */
static inline size_t countArgsAndFindReturnType(const char* descriptor,
    const char** pReturnType)
{
    size_t count = 0;
    bool bogus = false;
    bool done = false;

    assert(*descriptor == '(');
    descriptor++;

    while (!done) {
        switch (*descriptor) {
            case 'B': case 'C': case 'D': case 'F':
            case 'I': case 'J': case 'S': case 'Z': {
                count++;
                break;
            }
            case '[': {
                do {
                    descriptor++;
                } while (*descriptor == '[');
                /*
                 * Don't increment count, as it will be taken care of
                 * by the next iteration. Also, decrement descriptor
                 * to compensate for the increment below the switch.
                 */
                descriptor--;
                break;
            }
            case 'L': {
                do {
                    descriptor++;
                } while ((*descriptor != ';') && (*descriptor != '\0'));
                count++;
                if (*descriptor == '\0') {
                    /* Bogus descriptor. */
                    done = true;
                    bogus = true;
                }
                break;
            }
            case ')': {
                /*
                 * Note: The loop will exit after incrementing descriptor
                 * one more time, so it then points at the return type.
                 */
                done = true;
                break;
            }
            default: {
                /* Bogus descriptor. */
                done = true;
                bogus = true;
                break;
            }
        }

        descriptor++;
    }

    if (bogus) {
        *pReturnType = NULL;
        return 0;
    }

    *pReturnType = descriptor;
    return count;
}

/*
 * Copy the argument types into the given array using the given buffer
 * for the contents.
 */
static inline void copyTypes(char* buffer, const char** argTypes,
    size_t argCount, const char* descriptor)
{
    size_t i;
    char c;

    /* Skip the '('. */
    descriptor++;

    for (i = 0; i < argCount; i++) {
        argTypes[i] = buffer;

        /* Copy all the array markers and one extra character. */
        do {
            c = *(descriptor++);
            *(buffer++) = c;
        } while (c == '[');

        if (c == 'L') {
            /* Copy the rest of a class name. */
            do {
                c = *(descriptor++);
                *(buffer++) = c;
            } while (c != ';');
        }

        *(buffer++) = '\0';
    }
}

/*
 * Look for a match in the given class. Returns the match if found
 * or NULL if not.
 */
static Method* findMethodInListByDescriptor(const ClassObject* clazz,
    bool findVirtual, bool isHier, const char* name, const char* descriptor)
{
    const char* returnType;
    size_t argCount = countArgsAndFindReturnType(descriptor, &returnType);

    if (returnType == NULL) {
        ALOGW("Bogus method descriptor: %s", descriptor);
        return NULL;
    }

    /*
     * Make buffer big enough for all the argument type characters and
     * one '\0' per argument. The "- 2" is because "returnType -
     * descriptor" includes two parens.
     */
    char buffer[argCount + (returnType - descriptor) - 2];
    const char* argTypes[argCount];

    copyTypes(buffer, argTypes, argCount, descriptor);

    while (clazz != NULL) {
        Method* methods;
        size_t methodCount;
        size_t i;

        if (findVirtual) {
            methods = clazz->virtualMethods;
            methodCount = clazz->virtualMethodCount;
        } else {
            methods = clazz->directMethods;
            methodCount = clazz->directMethodCount;
        }

        for (i = 0; i < methodCount; i++) {
            Method* method = &methods[i];
            if (compareMethodHelper(method, name, returnType, argCount,
                            argTypes) == 0) {
                return method;
            }
        }

        if (! isHier) {
            break;
        }

        clazz = clazz->super;
    }

    return NULL;
}

/*
 * Look for a match in the given clazz. Returns the match if found
 * or NULL if not.
 *
 * "wantedType" should be METHOD_VIRTUAL or METHOD_DIRECT to indicate the
 * list to search through.  If the match can come from either list, use
 * MATCH_UNKNOWN to scan both.
 */
static Method* findMethodInListByProto(const ClassObject* clazz,
    MethodType wantedType, bool isHier, const char* name, const DexProto* proto)
{
    while (clazz != NULL) {
        int i;

        /*
         * Check the virtual and/or direct method lists.
         */
        if (wantedType == METHOD_VIRTUAL || wantedType == METHOD_UNKNOWN) {
            for (i = 0; i < clazz->virtualMethodCount; i++) {
                Method* method = &clazz->virtualMethods[i];
                if (dvmCompareNameProtoAndMethod(name, proto, method) == 0) {
                    return method;
                }
            }
        }
        if (wantedType == METHOD_DIRECT || wantedType == METHOD_UNKNOWN) {
            for (i = 0; i < clazz->directMethodCount; i++) {
                Method* method = &clazz->directMethods[i];
                if (dvmCompareNameProtoAndMethod(name, proto, method) == 0) {
                    return method;
                }
            }
        }

        if (! isHier) {
            break;
        }

        clazz = clazz->super;
    }

    return NULL;
}

/*
 * Find a "virtual" method in a class.
 *
 * Does not chase into the superclass.
 *
 * Returns NULL if the method can't be found.  (Does not throw an exception.)
 */
Method* dvmFindVirtualMethodByDescriptor(const ClassObject* clazz,
    const char* methodName, const char* descriptor)
{
    return findMethodInListByDescriptor(clazz, true, false,
            methodName, descriptor);

    // TODO? - throw IncompatibleClassChangeError if a match is
    // found in the directMethods list, rather than NotFoundError.
    // Note we could have been called by dvmFindVirtualMethodHier though.
}


/*
 * Find a "virtual" method in a class, knowing only the name.  This is
 * only useful in limited circumstances, e.g. when searching for a member
 * of an annotation class.
 *
 * Does not chase into the superclass.
 *
 * Returns NULL if the method can't be found.  (Does not throw an exception.)
 */
Method* dvmFindVirtualMethodByName(const ClassObject* clazz,
    const char* methodName)
{
    Method* methods = clazz->virtualMethods;
    int methodCount = clazz->virtualMethodCount;
    int i;

    for (i = 0; i < methodCount; i++) {
        if (strcmp(methods[i].name, methodName) == 0)
            return &methods[i];
    }

    return NULL;
}

/*
 * Find a "virtual" method in a class.
 *
 * Does not chase into the superclass.
 *
 * Returns NULL if the method can't be found.  (Does not throw an exception.)
 */
Method* dvmFindVirtualMethod(const ClassObject* clazz, const char* methodName,
    const DexProto* proto)
{
    return findMethodInListByProto(clazz, METHOD_VIRTUAL, false, methodName,
            proto);
}

/*
 * Find a "virtual" method in a class.  If we don't find it, try the
 * superclass.  Does not examine interfaces.
 *
 * Returns NULL if the method can't be found.  (Does not throw an exception.)
 */
Method* dvmFindVirtualMethodHierByDescriptor(const ClassObject* clazz,
    const char* methodName, const char* descriptor)
{
    return findMethodInListByDescriptor(clazz, true, true,
            methodName, descriptor);
}

/*
 * Find a "virtual" method in a class.  If we don't find it, try the
 * superclass.  Does not examine interfaces.
 *
 * Returns NULL if the method can't be found.  (Does not throw an exception.)
 */
Method* dvmFindVirtualMethodHier(const ClassObject* clazz,
    const char* methodName, const DexProto* proto)
{
    return findMethodInListByProto(clazz, METHOD_VIRTUAL, true, methodName,
            proto);
}

/*
 * Find a method in an interface.  Searches superinterfaces.
 *
 * Returns NULL if the method can't be found.  (Does not throw an exception.)
 */
Method* dvmFindInterfaceMethodHierByDescriptor(const ClassObject* iface,
    const char* methodName, const char* descriptor)
{
    Method* resMethod = dvmFindVirtualMethodByDescriptor(iface,
        methodName, descriptor);
    if (resMethod == NULL) {
        /* scan superinterfaces and superclass interfaces */
        int i;
        for (i = 0; i < iface->iftableCount; i++) {
            resMethod = dvmFindVirtualMethodByDescriptor(iface->iftable[i].clazz,
                methodName, descriptor);
            if (resMethod != NULL)
                break;
        }
    }
    return resMethod;
}

/*
 * Find a method in an interface.  Searches superinterfaces.
 *
 * Returns NULL if the method can't be found.  (Does not throw an exception.)
 */
Method* dvmFindInterfaceMethodHier(const ClassObject* iface,
    const char* methodName, const DexProto* proto)
{
    Method* resMethod = dvmFindVirtualMethod(iface, methodName, proto);
    if (resMethod == NULL) {
        /* scan superinterfaces and superclass interfaces */
        int i;
        for (i = 0; i < iface->iftableCount; i++) {
            resMethod = dvmFindVirtualMethod(iface->iftable[i].clazz,
                methodName, proto);
            if (resMethod != NULL)
                break;
        }
    }
    return resMethod;
}

/*
 * Find a "direct" method (static, private, or "<*init>").
 *
 * Returns NULL if the method can't be found.  (Does not throw an exception.)
 */
Method* dvmFindDirectMethodByDescriptor(const ClassObject* clazz,
    const char* methodName, const char* descriptor)
{
    return findMethodInListByDescriptor(clazz, false, false,
            methodName, descriptor);
}

/*
 * Find a "direct" method.  If we don't find it, try the superclass.  This
 * is only appropriate for static methods, but will work for all direct
 * methods.
 *
 * Returns NULL if the method can't be found.  (Does not throw an exception.)
 */
Method* dvmFindDirectMethodHierByDescriptor(const ClassObject* clazz,
    const char* methodName, const char* descriptor)
{
    return findMethodInListByDescriptor(clazz, false, true,
            methodName, descriptor);
}

/*
 * Find a "direct" method (static or "<*init>").
 *
 * Returns NULL if the method can't be found.  (Does not throw an exception.)
 */
Method* dvmFindDirectMethod(const ClassObject* clazz, const char* methodName,
    const DexProto* proto)
{
    return findMethodInListByProto(clazz, METHOD_DIRECT, false, methodName,
            proto);
}

/*
 * Find a "direct" method in a class.  If we don't find it, try the
 * superclass.
 *
 * Returns NULL if the method can't be found.  (Does not throw an exception.)
 */
Method* dvmFindDirectMethodHier(const ClassObject* clazz,
    const char* methodName, const DexProto* proto)
{
    return findMethodInListByProto(clazz, METHOD_DIRECT, true, methodName,
            proto);
}

/*
 * Find a virtual or static method in a class.  If we don't find it, try the
 * superclass.  This is compatible with the VM spec (v2 5.4.3.3) method
 * search order, but it stops short of scanning through interfaces (which
 * should be done after this function completes).
 *
 * In most cases we know that we're looking for either a static or an
 * instance field and there's no value in searching through both types.
 * During verification we need to recognize and reject certain unusual
 * situations, and we won't see them unless we walk the lists this way.
 *
 * Returns NULL if the method can't be found.  (Does not throw an exception.)
 */
Method* dvmFindMethodHier(const ClassObject* clazz, const char* methodName,
    const DexProto* proto)
{
    return findMethodInListByProto(clazz, METHOD_UNKNOWN, true, methodName,
            proto);
}


/*
 * We have a method pointer for a method in "clazz", but it might be
 * pointing to a method in a derived class.  We want to find the actual entry
 * from the class' vtable.  If "clazz" is an interface, we have to do a
 * little more digging.
 *
 * For "direct" methods (private / constructor), we just return the
 * original Method.
 *
 * (This is used for reflection and JNI "call method" calls.)
 */
const Method* dvmGetVirtualizedMethod(const ClassObject* clazz,
    const Method* meth)
{
    Method* actualMeth;
    int methodIndex;

    if (dvmIsDirectMethod(meth)) {
        /* no vtable entry for these */
        assert(!dvmIsStaticMethod(meth));
        return meth;
    }

    /*
     * If the method was declared in an interface, we need to scan through
     * the class' list of interfaces for it, and find the vtable index
     * from that.
     *
     * TODO: use the interface cache.
     */
    if (dvmIsInterfaceClass(meth->clazz)) {
        int i;

        for (i = 0; i < clazz->iftableCount; i++) {
            if (clazz->iftable[i].clazz == meth->clazz)
                break;
        }
        if (i == clazz->iftableCount) {
            dvmThrowIncompatibleClassChangeError(
                "invoking method from interface not implemented by class");
            return NULL;
        }

        methodIndex = clazz->iftable[i].methodIndexArray[meth->methodIndex];
    } else {
        methodIndex = meth->methodIndex;
    }

    assert(methodIndex >= 0 && methodIndex < clazz->vtableCount);
    actualMeth = clazz->vtable[methodIndex];

    /*
     * Make sure there's code to execute.
     */
    if (dvmIsAbstractMethod(actualMeth)) {
        dvmThrowAbstractMethodError(NULL);
        return NULL;
    }
    assert(!dvmIsMirandaMethod(actualMeth));

    return actualMeth;
}

/*
 * Get the source file for a method.
 */
const char* dvmGetMethodSourceFile(const Method* meth)
{
    /*
     * TODO: A method's debug info can override the default source
     * file for a class, so we should account for that possibility
     * here.
     */
    return meth->clazz->sourceFile;
}

/*
 * Dump some information about an object.
 */
void dvmDumpObject(const Object* obj)
{
    ClassObject* clazz;
    int i;

    if (obj == NULL || obj->clazz == NULL) {
        ALOGW("Null or malformed object not dumped");
        return;
    }

    clazz = obj->clazz;
    ALOGD("----- Object dump: %p (%s, %d bytes) -----",
        obj, clazz->descriptor, (int) clazz->objectSize);
    //printHexDump(obj, clazz->objectSize);
    ALOGD("  Fields:");
    while (clazz != NULL) {
        ALOGD("    -- %s", clazz->descriptor);
        for (i = 0; i < clazz->ifieldCount; i++) {
            const InstField* pField = &clazz->ifields[i];
            char type = pField->signature[0];

            if (type == 'F' || type == 'D') {
                double dval;

                if (type == 'F')
                    dval = dvmGetFieldFloat(obj, pField->byteOffset);
                else
                    dval = dvmGetFieldDouble(obj, pField->byteOffset);

                ALOGD("    %2d: '%s' '%s' af=%04x off=%d %.3f", i,
                    pField->name, pField->signature,
                    pField->accessFlags, pField->byteOffset, dval);
            } else {
                u8 lval;

                if (type == 'J')
                    lval = dvmGetFieldLong(obj, pField->byteOffset);
                else if (type == 'Z')
                    lval = dvmGetFieldBoolean(obj, pField->byteOffset);
                else
                    lval = dvmGetFieldInt(obj, pField->byteOffset);

                ALOGD("    %2d: '%s' '%s' af=%04x off=%d 0x%08llx", i,
                    pField->name, pField->signature,
                    pField->accessFlags, pField->byteOffset, lval);
            }
        }

        clazz = clazz->super;
    }
    if (dvmIsClassObject(obj)) {
        ALOGD("  Static fields:");
        const StaticField* sfields = &((ClassObject *)obj)->sfields[0];
        for (i = 0; i < ((ClassObject *)obj)->sfieldCount; ++i) {
            const StaticField* pField = &sfields[i];
            size_t byteOffset = (size_t)pField - (size_t)sfields;
            char type = pField->signature[0];

            if (type == 'F' || type == 'D') {
                double dval;

                if (type == 'F')
                    dval = pField->value.f;
                else
                    dval = pField->value.d;

                ALOGD("    %2d: '%s' '%s' af=%04x off=%zd %.3f", i,
                     pField->name, pField->signature,
                     pField->accessFlags, byteOffset, dval);
            } else {
                u8 lval;

                if (type == 'J')
                    lval = pField->value.j;
                else if (type == 'Z')
                    lval = pField->value.z;
                else
                    lval = pField->value.i;

                ALOGD("    %2d: '%s' '%s' af=%04x off=%zd 0x%08llx", i,
                     pField->name, pField->signature,
                     pField->accessFlags, byteOffset, lval);
            }
        }
    }
}
