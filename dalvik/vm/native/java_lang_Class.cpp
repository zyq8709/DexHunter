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
 * java.lang.Class
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"
#include "ScopedPthreadMutexLock.h"

/*
 * native public boolean desiredAssertionStatus()
 *
 * Determine the class-init-time assertion status of a class.  This is
 * called from <clinit> in javac-generated classes that use the Java
 * programming language "assert" keyword.
 */
static void Dalvik_java_lang_Class_desiredAssertionStatus(const u4* args,
    JValue* pResult)
{
    ClassObject* thisPtr = (ClassObject*) args[0];
    char* className = dvmDescriptorToName(thisPtr->descriptor);
    int i;
    bool enable = false;

    /*
     * Run through the list of arguments specified on the command line.  The
     * last matching argument takes precedence.
     */
    for (i = 0; i < gDvm.assertionCtrlCount; i++) {
        const AssertionControl* pCtrl = &gDvm.assertionCtrl[i];

        if (pCtrl->isPackage) {
            /*
             * Given "dalvik/system/Debug" or "MyStuff", compute the
             * length of the package portion of the class name string.
             *
             * Unlike most package operations, we allow matching on
             * "sub-packages", so "dalvik..." will match "dalvik.Foo"
             * and "dalvik.system.Foo".
             *
             * The pkgOrClass string looks like "dalvik/system/", i.e. it still
             * has the terminating slash, so we can be sure we're comparing
             * against full package component names.
             */
            const char* lastSlash;
            int pkgLen;

            lastSlash = strrchr(className, '/');
            if (lastSlash == NULL) {
                pkgLen = 0;
            } else {
                pkgLen = lastSlash - className +1;
            }

            if (pCtrl->pkgOrClassLen > pkgLen ||
                memcmp(pCtrl->pkgOrClass, className, pCtrl->pkgOrClassLen) != 0)
            {
                ALOGV("ASRT: pkg no match: '%s'(%d) vs '%s'",
                    className, pkgLen, pCtrl->pkgOrClass);
            } else {
                ALOGV("ASRT: pkg match: '%s'(%d) vs '%s' --> %d",
                    className, pkgLen, pCtrl->pkgOrClass, pCtrl->enable);
                enable = pCtrl->enable;
            }
        } else {
            /*
             * "pkgOrClass" holds a fully-qualified class name, converted from
             * dot-form to slash-form.  An empty string means all classes.
             */
            if (pCtrl->pkgOrClass == NULL) {
                /* -esa/-dsa; see if class is a "system" class */
                if (strncmp(className, "java/", 5) != 0) {
                    ALOGV("ASRT: sys no match: '%s'", className);
                } else {
                    ALOGV("ASRT: sys match: '%s' --> %d",
                        className, pCtrl->enable);
                    enable = pCtrl->enable;
                }
            } else if (*pCtrl->pkgOrClass == '\0') {
                ALOGV("ASRT: class all: '%s' --> %d",
                    className, pCtrl->enable);
                enable = pCtrl->enable;
            } else {
                if (strcmp(pCtrl->pkgOrClass, className) != 0) {
                    ALOGV("ASRT: cls no match: '%s' vs '%s'",
                        className, pCtrl->pkgOrClass);
                } else {
                    ALOGV("ASRT: cls match: '%s' vs '%s' --> %d",
                        className, pCtrl->pkgOrClass, pCtrl->enable);
                    enable = pCtrl->enable;
                }
            }
        }
    }

    free(className);
    RETURN_INT(enable);
}

/*
 * static public Class<?> classForName(String name, boolean initialize,
 *     ClassLoader loader)
 *
 * Return the Class object associated with the class or interface with
 * the specified name.
 *
 * "name" is in "binary name" format, e.g. "dalvik.system.Debug$1".
 */
static void Dalvik_java_lang_Class_classForName(const u4* args, JValue* pResult)
{
    StringObject* nameObj = (StringObject*) args[0];
    bool initialize = (args[1] != 0);
    Object* loader = (Object*) args[2];

    RETURN_PTR(dvmFindClassByName(nameObj, loader, initialize));
}

/*
 * static private ClassLoader getClassLoader(Class clazz)
 *
 * Return the class' defining class loader.
 */
static void Dalvik_java_lang_Class_getClassLoader(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];

    RETURN_PTR(clazz->classLoader);
}

/*
 * public Class<?> getComponentType()
 *
 * If this is an array type, return the class of the elements; otherwise
 * return NULL.
 */
static void Dalvik_java_lang_Class_getComponentType(const u4* args,
    JValue* pResult)
{
    ClassObject* thisPtr = (ClassObject*) args[0];

    if (!dvmIsArrayClass(thisPtr))
        RETURN_PTR(NULL);

    /*
     * We can't just return thisPtr->elementClass, because that gives
     * us the base type (e.g. X[][][] returns X).  If this is a multi-
     * dimensional array, we have to do the lookup by name.
     */
    if (thisPtr->descriptor[1] == '[')
        RETURN_PTR(dvmFindArrayClass(&thisPtr->descriptor[1],
                   thisPtr->classLoader));
    else
        RETURN_PTR(thisPtr->elementClass);
}

/*
 * private static Class<?>[] getDeclaredClasses(Class<?> clazz,
 *     boolean publicOnly)
 *
 * Return an array with the classes that are declared by the specified class.
 * If "publicOnly" is set, we strip out any classes that don't have "public"
 * access.
 */
static void Dalvik_java_lang_Class_getDeclaredClasses(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    bool publicOnly = (args[1] != 0);
    ArrayObject* classes;

    classes = dvmGetDeclaredClasses(clazz);
    if (classes == NULL) {
        if (!dvmCheckException(dvmThreadSelf())) {
            /* empty list, so create a zero-length array */
            classes = dvmAllocArrayByClass(gDvm.classJavaLangClassArray,
                        0, ALLOC_DEFAULT);
        }
    } else if (publicOnly) {
        u4 count, newIdx, publicCount = 0;
        ClassObject** pSource = (ClassObject**)(void*)classes->contents;
        u4 length = classes->length;

        /* count up public classes */
        for (count = 0; count < length; count++) {
            if (dvmIsPublicClass(pSource[count]))
                publicCount++;
        }

        /* create a new array to hold them */
        ArrayObject* newClasses;
        newClasses = dvmAllocArrayByClass(gDvm.classJavaLangClassArray,
                        publicCount, ALLOC_DEFAULT);

        /* copy them over */
        for (count = newIdx = 0; count < length; count++) {
            if (dvmIsPublicClass(pSource[count])) {
                dvmSetObjectArrayElement(newClasses, newIdx,
                                         (Object *)pSource[count]);
                newIdx++;
            }
        }
        assert(newIdx == publicCount);
        dvmReleaseTrackedAlloc((Object*) classes, NULL);
        classes = newClasses;
    }

    dvmReleaseTrackedAlloc((Object*) classes, NULL);
    RETURN_PTR(classes);
}

/*
 * static Constructor[] getDeclaredConstructors(Class clazz, boolean publicOnly)
 *     throws SecurityException
 */
static void Dalvik_java_lang_Class_getDeclaredConstructors(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    bool publicOnly = (args[1] != 0);
    ArrayObject* constructors;

    constructors = dvmGetDeclaredConstructors(clazz, publicOnly);
    dvmReleaseTrackedAlloc((Object*) constructors, NULL);

    RETURN_PTR(constructors);
}

/*
 * static Field[] getDeclaredFields(Class klass, boolean publicOnly)
 */
static void Dalvik_java_lang_Class_getDeclaredFields(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    bool publicOnly = (args[1] != 0);
    ArrayObject* fields;

    fields = dvmGetDeclaredFields(clazz, publicOnly);
    dvmReleaseTrackedAlloc((Object*) fields, NULL);

    RETURN_PTR(fields);
}

/*
 * static Field getDeclaredField(Class klass, String name)
 */
static void Dalvik_java_lang_Class_getDeclaredField(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    StringObject* nameObj = (StringObject*) args[1];
    Object* fieldObj = dvmGetDeclaredField(clazz, nameObj);
    dvmReleaseTrackedAlloc((Object*) fieldObj, NULL);
    RETURN_PTR(fieldObj);
}

/*
 * static Method[] getDeclaredMethods(Class clazz, boolean publicOnly)
 *     throws SecurityException
 */
static void Dalvik_java_lang_Class_getDeclaredMethods(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    bool publicOnly = (args[1] != 0);
    ArrayObject* methods;

    methods = dvmGetDeclaredMethods(clazz, publicOnly);
    dvmReleaseTrackedAlloc((Object*) methods, NULL);

    RETURN_PTR(methods);
}

/*
 * static native Member getDeclaredConstructorOrMethod(
 *     Class clazz, String name, Class[] args);
 */
static void Dalvik_java_lang_Class_getDeclaredConstructorOrMethod(
    const u4* args, JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    StringObject* nameObj = (StringObject*) args[1];
    ArrayObject* methodArgs = (ArrayObject*) args[2];

    Object* methodObj;

    methodObj = dvmGetDeclaredConstructorOrMethod(clazz, nameObj, methodArgs);
    dvmReleaseTrackedAlloc(methodObj, NULL);

    RETURN_PTR(methodObj);
}

/*
 * Class[] getInterfaces()
 */
static void Dalvik_java_lang_Class_getInterfaces(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    ArrayObject* interfaces;

    interfaces = dvmGetInterfaces(clazz);
    dvmReleaseTrackedAlloc((Object*) interfaces, NULL);

    RETURN_PTR(interfaces);
}

/*
 * private static int getModifiers(Class klass, boolean
 *     ignoreInnerClassesAttrib)
 *
 * Return the class' modifier flags.  If "ignoreInnerClassesAttrib" is false,
 * and this is an inner class, we return the access flags from the inner class
 * attribute.
 */
static void Dalvik_java_lang_Class_getModifiers(const u4* args, JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    bool ignoreInner = args[1];
    u4 accessFlags;

    accessFlags = clazz->accessFlags & JAVA_FLAGS_MASK;

    if (!ignoreInner) {
        /* see if we have an InnerClass annotation with flags in it */
        StringObject* className = NULL;
        int innerFlags;

        if (dvmGetInnerClass(clazz, &className, &innerFlags))
            accessFlags = innerFlags & JAVA_FLAGS_MASK;

        dvmReleaseTrackedAlloc((Object*) className, NULL);
    }

    RETURN_INT(accessFlags);
}

/*
 * private native String getNameNative()
 *
 * Return the class' name. The exact format is bizarre, but it's the specified
 * behavior: keywords for primitive types, regular "[I" form for primitive
 * arrays (so "int" but "[I"), and arrays of reference types written
 * between "L" and ";" but with dots rather than slashes (so "java.lang.String"
 * but "[Ljava.lang.String;"). Madness.
 */
static void Dalvik_java_lang_Class_getNameNative(const u4* args, JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    const char* descriptor = clazz->descriptor;
    StringObject* nameObj;

    if ((descriptor[0] != 'L') && (descriptor[0] != '[')) {
        /*
         * The descriptor indicates that this is the class for
         * a primitive type; special-case the return value.
         */
        const char* name;
        switch (descriptor[0]) {
            case 'Z': name = "boolean"; break;
            case 'B': name = "byte";    break;
            case 'C': name = "char";    break;
            case 'S': name = "short";   break;
            case 'I': name = "int";     break;
            case 'J': name = "long";    break;
            case 'F': name = "float";   break;
            case 'D': name = "double";  break;
            case 'V': name = "void";    break;
            default: {
                ALOGE("Unknown primitive type '%c'", descriptor[0]);
                assert(false);
                RETURN_PTR(NULL);
            }
        }

        nameObj = dvmCreateStringFromCstr(name);
    } else {
        /*
         * Convert the UTF-8 name to a java.lang.String. The
         * name must use '.' to separate package components.
         *
         * TODO: this could be more efficient. Consider a custom
         * conversion function here that walks the string once and
         * avoids the allocation for the common case (name less than,
         * say, 128 bytes).
         */
        char* dotName = dvmDescriptorToDot(clazz->descriptor);
        nameObj = dvmCreateStringFromCstr(dotName);
        free(dotName);
    }

    dvmReleaseTrackedAlloc((Object*) nameObj, NULL);
    RETURN_PTR(nameObj);
}

/*
 * Return the superclass for instances of this class.
 *
 * If the class represents a java/lang/Object, an interface, a primitive
 * type, or void (which *is* a primitive type??), return NULL.
 *
 * For an array, return the java/lang/Object ClassObject.
 */
static void Dalvik_java_lang_Class_getSuperclass(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];

    if (dvmIsPrimitiveClass(clazz) || dvmIsInterfaceClass(clazz))
        RETURN_PTR(NULL);
    else
        RETURN_PTR(clazz->super);
}

/*
 * public boolean isAssignableFrom(Class<?> cls)
 *
 * Determine if this class is either the same as, or is a superclass or
 * superinterface of, the class specified in the "cls" parameter.
 */
static void Dalvik_java_lang_Class_isAssignableFrom(const u4* args,
    JValue* pResult)
{
    ClassObject* thisPtr = (ClassObject*) args[0];
    ClassObject* testClass = (ClassObject*) args[1];

    if (testClass == NULL) {
        dvmThrowNullPointerException("cls == null");
        RETURN_INT(false);
    }
    RETURN_INT(dvmInstanceof(testClass, thisPtr));
}

/*
 * public boolean isInstance(Object o)
 *
 * Dynamic equivalent of Java programming language "instanceof".
 */
static void Dalvik_java_lang_Class_isInstance(const u4* args,
    JValue* pResult)
{
    ClassObject* thisPtr = (ClassObject*) args[0];
    Object* testObj = (Object*) args[1];

    if (testObj == NULL)
        RETURN_INT(false);
    RETURN_INT(dvmInstanceof(testObj->clazz, thisPtr));
}

/*
 * public boolean isInterface()
 */
static void Dalvik_java_lang_Class_isInterface(const u4* args,
    JValue* pResult)
{
    ClassObject* thisPtr = (ClassObject*) args[0];

    RETURN_INT(dvmIsInterfaceClass(thisPtr));
}

/*
 * public boolean isPrimitive()
 */
static void Dalvik_java_lang_Class_isPrimitive(const u4* args,
    JValue* pResult)
{
    ClassObject* thisPtr = (ClassObject*) args[0];

    RETURN_INT(dvmIsPrimitiveClass(thisPtr));
}

/*
 * public T newInstance() throws InstantiationException, IllegalAccessException
 *
 * Create a new instance of this class.
 */
static void Dalvik_java_lang_Class_newInstance(const u4* args, JValue* pResult)
{
    Thread* self = dvmThreadSelf();
    ClassObject* clazz = (ClassObject*) args[0];
    Method* init;
    Object* newObj;

    /* can't instantiate these */
    if (dvmIsPrimitiveClass(clazz) || dvmIsInterfaceClass(clazz)
        || dvmIsArrayClass(clazz) || dvmIsAbstractClass(clazz))
    {
        ALOGD("newInstance failed: p%d i%d [%d a%d",
            dvmIsPrimitiveClass(clazz), dvmIsInterfaceClass(clazz),
            dvmIsArrayClass(clazz), dvmIsAbstractClass(clazz));
        dvmThrowInstantiationException(clazz, NULL);
        RETURN_VOID();
    }

    /* initialize the class if it hasn't been already */
    if (!dvmIsClassInitialized(clazz)) {
        if (!dvmInitClass(clazz)) {
            ALOGW("Class init failed in newInstance call (%s)",
                clazz->descriptor);
            assert(dvmCheckException(self));
            RETURN_VOID();
        }
    }

    /* find the "nullary" constructor */
    init = dvmFindDirectMethodByDescriptor(clazz, "<init>", "()V");
    if (init == NULL) {
        /* common cause: secret "this" arg on non-static inner class ctor */
        ALOGD("newInstance failed: no <init>()");
        dvmThrowInstantiationException(clazz, "no empty constructor");
        RETURN_VOID();
    }

    /*
     * Verify access from the call site.
     *
     * First, make sure the method invoking Class.newInstance() has permission
     * to access the class.
     *
     * Second, make sure it has permission to invoke the constructor.  The
     * constructor must be public or, if the caller is in the same package,
     * have package scope.
     */
    ClassObject* callerClass = dvmGetCaller2Class(self->interpSave.curFrame);

    if (!dvmCheckClassAccess(callerClass, clazz)) {
        ALOGD("newInstance failed: %s not accessible to %s",
            clazz->descriptor, callerClass->descriptor);
        dvmThrowIllegalAccessException("access to class not allowed");
        RETURN_VOID();
    }
    if (!dvmCheckMethodAccess(callerClass, init)) {
        ALOGD("newInstance failed: %s.<init>() not accessible to %s",
            clazz->descriptor, callerClass->descriptor);
        dvmThrowIllegalAccessException("access to constructor not allowed");
        RETURN_VOID();
    }

    newObj = dvmAllocObject(clazz, ALLOC_DEFAULT);
    JValue unused;

    /* invoke constructor; unlike reflection calls, we don't wrap exceptions */
    dvmCallMethod(self, init, newObj, &unused);
    dvmReleaseTrackedAlloc(newObj, NULL);

    RETURN_PTR(newObj);
}

/*
 * private Object[] getSignatureAnnotation()
 *
 * Returns the signature annotation array.
 */
static void Dalvik_java_lang_Class_getSignatureAnnotation(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    ArrayObject* arr = dvmGetClassSignatureAnnotation(clazz);

    dvmReleaseTrackedAlloc((Object*) arr, NULL);
    RETURN_PTR(arr);
}

/*
 * public Class getDeclaringClass()
 *
 * Get the class that encloses this class (if any).
 */
static void Dalvik_java_lang_Class_getDeclaringClass(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];

    ClassObject* enclosing = dvmGetDeclaringClass(clazz);
    dvmReleaseTrackedAlloc((Object*) enclosing, NULL);
    RETURN_PTR(enclosing);
}

/*
 * public Class getEnclosingClass()
 *
 * Get the class that encloses this class (if any).
 */
static void Dalvik_java_lang_Class_getEnclosingClass(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];

    ClassObject* enclosing = dvmGetEnclosingClass(clazz);
    dvmReleaseTrackedAlloc((Object*) enclosing, NULL);
    RETURN_PTR(enclosing);
}

/*
 * public Constructor getEnclosingConstructor()
 *
 * Get the constructor that encloses this class (if any).
 */
static void Dalvik_java_lang_Class_getEnclosingConstructor(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];

    Object* enclosing = dvmGetEnclosingMethod(clazz);
    if (enclosing != NULL) {
        dvmReleaseTrackedAlloc(enclosing, NULL);
        if (enclosing->clazz == gDvm.classJavaLangReflectConstructor) {
            RETURN_PTR(enclosing);
        }
        assert(enclosing->clazz == gDvm.classJavaLangReflectMethod);
    }
    RETURN_PTR(NULL);
}

/*
 * public Method getEnclosingMethod()
 *
 * Get the method that encloses this class (if any).
 */
static void Dalvik_java_lang_Class_getEnclosingMethod(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];

    Object* enclosing = dvmGetEnclosingMethod(clazz);
    if (enclosing != NULL) {
        dvmReleaseTrackedAlloc(enclosing, NULL);
        if (enclosing->clazz == gDvm.classJavaLangReflectMethod) {
            RETURN_PTR(enclosing);
        }
        assert(enclosing->clazz == gDvm.classJavaLangReflectConstructor);
    }
    RETURN_PTR(NULL);
}

#if 0
static void Dalvik_java_lang_Class_getGenericInterfaces(const u4* args,
    JValue* pResult)
{
    dvmThrowUnsupportedOperationException("native method not implemented");

    RETURN_PTR(NULL);
}

static void Dalvik_java_lang_Class_getGenericSuperclass(const u4* args,
    JValue* pResult)
{
    dvmThrowUnsupportedOperationException("native method not implemented");

    RETURN_PTR(NULL);
}

static void Dalvik_java_lang_Class_getTypeParameters(const u4* args,
    JValue* pResult)
{
    dvmThrowUnsupportedOperationException("native method not implemented");

    RETURN_PTR(NULL);
}
#endif

/*
 * public boolean isAnonymousClass()
 *
 * Returns true if this is an "anonymous" class.
 */
static void Dalvik_java_lang_Class_isAnonymousClass(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    StringObject* className = NULL;
    int accessFlags;

    /*
     * If this has an InnerClass annotation, pull it out.  Lack of the
     * annotation, or an annotation with a NULL class name, indicates
     * that this is an anonymous inner class.
     */
    if (!dvmGetInnerClass(clazz, &className, &accessFlags))
        RETURN_BOOLEAN(false);

    dvmReleaseTrackedAlloc((Object*) className, NULL);
    RETURN_BOOLEAN(className == NULL);
}

/*
 * private Annotation[] getDeclaredAnnotations()
 *
 * Return the annotations declared on this class.
 */
static void Dalvik_java_lang_Class_getDeclaredAnnotations(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];

    ArrayObject* annos = dvmGetClassAnnotations(clazz);
    dvmReleaseTrackedAlloc((Object*) annos, NULL);
    RETURN_PTR(annos);
}

/*
 * private Annotation getDeclaredAnnotation(Class annotationClass)
 */
static void Dalvik_java_lang_Class_getDeclaredAnnotation(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    ClassObject* annotationClazz = (ClassObject*) args[1];

    RETURN_PTR(dvmGetClassAnnotation(clazz, annotationClazz));
}

/*
 * private boolean isDeclaredAnnotationPresent(Class annotationClass);
 */
static void Dalvik_java_lang_Class_isDeclaredAnnotationPresent(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    ClassObject* annotationClazz = (ClassObject*) args[1];

    RETURN_BOOLEAN(dvmIsClassAnnotationPresent(clazz, annotationClazz));
}

/*
 * public String getInnerClassName()
 *
 * Returns the simple name of a member class or local class, or null otherwise.
 */
static void Dalvik_java_lang_Class_getInnerClassName(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    StringObject* nameObj;
    int flags;

    if (dvmGetInnerClass(clazz, &nameObj, &flags)) {
        dvmReleaseTrackedAlloc((Object*) nameObj, NULL);
        RETURN_PTR(nameObj);
    } else {
        RETURN_PTR(NULL);
    }
}

JNIEXPORT jobject JNICALL Java_java_lang_Class_getDex(JNIEnv* env, jclass javaClass) {
    Thread* self = dvmThreadSelf();
    ClassObject* c = (ClassObject*) dvmDecodeIndirectRef(self, javaClass);

    DvmDex* dvm_dex = c->pDvmDex;
    if (dvm_dex == NULL) {
        return NULL;
    }
    // Already cached?
    if (dvm_dex->dex_object != NULL) {
        return dvm_dex->dex_object;
    }
    jobject byte_buffer = env->NewDirectByteBuffer(dvm_dex->memMap.addr, dvm_dex->memMap.length);
    if (byte_buffer == NULL) {
        return NULL;
    }

    jclass com_android_dex_Dex = env->FindClass("com/android/dex/Dex");
    if (com_android_dex_Dex == NULL) {
        return NULL;
    }

    jmethodID com_android_dex_Dex_create =
            env->GetStaticMethodID(com_android_dex_Dex,
                                   "create", "(Ljava/nio/ByteBuffer;)Lcom/android/dex/Dex;");
    if (com_android_dex_Dex_create == NULL) {
        return NULL;
    }

    jvalue args[1];
    args[0].l = byte_buffer;
    jobject local_ref = env->CallStaticObjectMethodA(com_android_dex_Dex,
                                                     com_android_dex_Dex_create,
                                                     args);
    if (local_ref == NULL) {
        return NULL;
    }

    // Check another thread didn't cache an object, if we've won install the object.
    ScopedPthreadMutexLock lock(&dvm_dex->modLock);

    if (dvm_dex->dex_object == NULL) {
        dvm_dex->dex_object = env->NewGlobalRef(local_ref);
    }
    return dvm_dex->dex_object;
}

const DalvikNativeMethod dvm_java_lang_Class[] = {
    { "desiredAssertionStatus", "()Z",
        Dalvik_java_lang_Class_desiredAssertionStatus },
    { "classForName",           "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;",
        Dalvik_java_lang_Class_classForName },
    { "getClassLoader",         "(Ljava/lang/Class;)Ljava/lang/ClassLoader;",
        Dalvik_java_lang_Class_getClassLoader },
    { "getComponentType",       "()Ljava/lang/Class;",
        Dalvik_java_lang_Class_getComponentType },
    { "getSignatureAnnotation",  "()[Ljava/lang/Object;",
        Dalvik_java_lang_Class_getSignatureAnnotation },
    { "getDeclaredClasses",     "(Ljava/lang/Class;Z)[Ljava/lang/Class;",
        Dalvik_java_lang_Class_getDeclaredClasses },
    { "getDeclaredConstructors", "(Ljava/lang/Class;Z)[Ljava/lang/reflect/Constructor;",
        Dalvik_java_lang_Class_getDeclaredConstructors },
    { "getDeclaredFields",      "(Ljava/lang/Class;Z)[Ljava/lang/reflect/Field;",
        Dalvik_java_lang_Class_getDeclaredFields },
    { "getDeclaredMethods",     "(Ljava/lang/Class;Z)[Ljava/lang/reflect/Method;",
        Dalvik_java_lang_Class_getDeclaredMethods },
    { "getDeclaredField",      "(Ljava/lang/Class;Ljava/lang/String;)Ljava/lang/reflect/Field;",
        Dalvik_java_lang_Class_getDeclaredField },
    { "getDeclaredConstructorOrMethod", "(Ljava/lang/Class;Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Member;",
        Dalvik_java_lang_Class_getDeclaredConstructorOrMethod },
    { "getInterfaces",          "()[Ljava/lang/Class;",
        Dalvik_java_lang_Class_getInterfaces },
    { "getModifiers",           "(Ljava/lang/Class;Z)I",
        Dalvik_java_lang_Class_getModifiers },
    { "getNameNative",                "()Ljava/lang/String;",
        Dalvik_java_lang_Class_getNameNative },
    { "getSuperclass",          "()Ljava/lang/Class;",
        Dalvik_java_lang_Class_getSuperclass },
    { "isAssignableFrom",       "(Ljava/lang/Class;)Z",
        Dalvik_java_lang_Class_isAssignableFrom },
    { "isInstance",             "(Ljava/lang/Object;)Z",
        Dalvik_java_lang_Class_isInstance },
    { "isInterface",            "()Z",
        Dalvik_java_lang_Class_isInterface },
    { "isPrimitive",            "()Z",
        Dalvik_java_lang_Class_isPrimitive },
    { "newInstanceImpl",        "()Ljava/lang/Object;",
        Dalvik_java_lang_Class_newInstance },
    { "getDeclaringClass",      "()Ljava/lang/Class;",
        Dalvik_java_lang_Class_getDeclaringClass },
    { "getEnclosingClass",      "()Ljava/lang/Class;",
        Dalvik_java_lang_Class_getEnclosingClass },
    { "getEnclosingConstructor", "()Ljava/lang/reflect/Constructor;",
        Dalvik_java_lang_Class_getEnclosingConstructor },
    { "getEnclosingMethod",     "()Ljava/lang/reflect/Method;",
        Dalvik_java_lang_Class_getEnclosingMethod },
#if 0
    { "getGenericInterfaces",   "()[Ljava/lang/reflect/Type;",
        Dalvik_java_lang_Class_getGenericInterfaces },
    { "getGenericSuperclass",   "()Ljava/lang/reflect/Type;",
        Dalvik_java_lang_Class_getGenericSuperclass },
    { "getTypeParameters",      "()Ljava/lang/reflect/TypeVariable;",
        Dalvik_java_lang_Class_getTypeParameters },
#endif
    { "isAnonymousClass",       "()Z",
        Dalvik_java_lang_Class_isAnonymousClass },
    { "getDeclaredAnnotations", "()[Ljava/lang/annotation/Annotation;",
        Dalvik_java_lang_Class_getDeclaredAnnotations },
    { "getDeclaredAnnotation", "(Ljava/lang/Class;)Ljava/lang/annotation/Annotation;",
        Dalvik_java_lang_Class_getDeclaredAnnotation },
    { "isDeclaredAnnotationPresent", "(Ljava/lang/Class;)Z",
        Dalvik_java_lang_Class_isDeclaredAnnotationPresent },
    { "getInnerClassName",       "()Ljava/lang/String;",
        Dalvik_java_lang_Class_getInnerClassName },
    { NULL, NULL, NULL },
};
