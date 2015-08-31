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
 * java.lang.reflect.Method
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"


/*
 * static int getMethodModifiers(Class decl_class, int slot)
 *
 * (Not sure why the access flags weren't stored in the class along with
 * everything else.  Not sure why this isn't static.)
 */
static void Dalvik_java_lang_reflect_Method_getMethodModifiers(const u4* args,
    JValue* pResult)
{
    ClassObject* declaringClass = (ClassObject*) args[0];
    int slot = args[1];
    Method* meth;

    meth = dvmSlotToMethod(declaringClass, slot);
    RETURN_INT(dvmFixMethodFlags(meth->accessFlags));
}

/*
 * private Object invokeNative(Object obj, Object[] args, Class declaringClass,
 *   Class[] parameterTypes, Class returnType, int slot, boolean noAccessCheck)
 *
 * Invoke a static or virtual method via reflection.
 */
static void Dalvik_java_lang_reflect_Method_invokeNative(const u4* args,
    JValue* pResult)
{
    // ignore thisPtr in args[0]
    Object* methObj = (Object*) args[1];        // null for static methods
    ArrayObject* argList = (ArrayObject*) args[2];
    ClassObject* declaringClass = (ClassObject*) args[3];
    ArrayObject* params = (ArrayObject*) args[4];
    ClassObject* returnType = (ClassObject*) args[5];
    int slot = args[6];
    bool noAccessCheck = (args[7] != 0);
    const Method* meth;
    Object* result;

    /*
     * "If the underlying method is static, the class that declared the
     * method is initialized if it has not already been initialized."
     */
    meth = dvmSlotToMethod(declaringClass, slot);
    assert(meth != NULL);

    if (dvmIsStaticMethod(meth)) {
        if (!dvmIsClassInitialized(declaringClass)) {
            if (!dvmInitClass(declaringClass))
                goto init_failed;
        }
    } else {
        /* looks like interfaces need this too? */
        if (dvmIsInterfaceClass(declaringClass) &&
            !dvmIsClassInitialized(declaringClass))
        {
            if (!dvmInitClass(declaringClass))
                goto init_failed;
        }

        /* make sure the object is an instance of the expected class */
        if (!dvmVerifyObjectInClass(methObj, declaringClass)) {
            assert(dvmCheckException(dvmThreadSelf()));
            RETURN_VOID();
        }

        /* do the virtual table lookup for the method */
        meth = dvmGetVirtualizedMethod(methObj->clazz, meth);
        if (meth == NULL) {
            assert(dvmCheckException(dvmThreadSelf()));
            RETURN_VOID();
        }
    }

    /*
     * If the method has a return value, "result" will be an object or
     * a boxed primitive.
     */
    result = dvmInvokeMethod(methObj, meth, argList, params, returnType,
                noAccessCheck);

    RETURN_PTR(result);

init_failed:
    /*
     * If initialization failed, an exception will be raised.
     */
    ALOGD("Method.invoke() on bad class %s failed",
        declaringClass->descriptor);
    assert(dvmCheckException(dvmThreadSelf()));
    RETURN_VOID();
}

/*
 * static Annotation[] getDeclaredAnnotations(Class declaringClass, int slot)
 *
 * Return the annotations declared for this method.
 */
static void Dalvik_java_lang_reflect_Method_getDeclaredAnnotations(
    const u4* args, JValue* pResult)
{
    ClassObject* declaringClass = (ClassObject*) args[0];
    int slot = args[1];
    Method* meth;

    meth = dvmSlotToMethod(declaringClass, slot);
    assert(meth != NULL);

    ArrayObject* annos = dvmGetMethodAnnotations(meth);
    dvmReleaseTrackedAlloc((Object*)annos, NULL);
    RETURN_PTR(annos);
}

/*
 * static Annotation getAnnotation(
 *         Class declaringClass, int slot, Class annotationType);
 */
static void Dalvik_java_lang_reflect_Method_getAnnotation(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    int slot = args[1];
    ClassObject* annotationClazz = (ClassObject*) args[2];

    Method* meth = dvmSlotToMethod(clazz, slot);
    RETURN_PTR(dvmGetMethodAnnotation(clazz, meth, annotationClazz));
}

/*
 * static boolean isAnnotationPresent(
 *         Class declaringClass, int slot, Class annotationType);
 */
static void Dalvik_java_lang_reflect_Method_isAnnotationPresent(const u4* args,
    JValue* pResult)
{
    ClassObject* clazz = (ClassObject*) args[0];
    int slot = args[1];
    ClassObject* annotationClazz = (ClassObject*) args[2];

    Method* meth = dvmSlotToMethod(clazz, slot);
    RETURN_BOOLEAN(dvmIsMethodAnnotationPresent(clazz, meth, annotationClazz));
}

/*
 * static Annotation[][] getParameterAnnotations(Class declaringClass, int slot)
 *
 * Return the annotations declared for this method's parameters.
 */
static void Dalvik_java_lang_reflect_Method_getParameterAnnotations(
    const u4* args, JValue* pResult)
{
    ClassObject* declaringClass = (ClassObject*) args[0];
    int slot = args[1];
    Method* meth;

    meth = dvmSlotToMethod(declaringClass, slot);
    assert(meth != NULL);

    ArrayObject* annos = dvmGetParameterAnnotations(meth);
    dvmReleaseTrackedAlloc((Object*)annos, NULL);
    RETURN_PTR(annos);
}

/*
 * private Object getDefaultValue(Class declaringClass, int slot)
 *
 * Return the default value for the annotation member represented by
 * this Method instance.  Returns NULL if none is defined.
 */
static void Dalvik_java_lang_reflect_Method_getDefaultValue(const u4* args,
    JValue* pResult)
{
    // ignore thisPtr in args[0]
    ClassObject* declaringClass = (ClassObject*) args[1];
    int slot = args[2];
    Method* meth;

    /* make sure this is an annotation class member */
    if (!dvmIsAnnotationClass(declaringClass))
        RETURN_PTR(NULL);

    meth = dvmSlotToMethod(declaringClass, slot);
    assert(meth != NULL);

    Object* def = dvmGetAnnotationDefaultValue(meth);
    dvmReleaseTrackedAlloc(def, NULL);
    RETURN_PTR(def);
}

/*
 * static Object[] getSignatureAnnotation()
 *
 * Returns the signature annotation.
 */
static void Dalvik_java_lang_reflect_Method_getSignatureAnnotation(
    const u4* args, JValue* pResult)
{
    ClassObject* declaringClass = (ClassObject*) args[0];
    int slot = args[1];
    Method* meth;

    meth = dvmSlotToMethod(declaringClass, slot);
    assert(meth != NULL);

    ArrayObject* arr = dvmGetMethodSignatureAnnotation(meth);
    dvmReleaseTrackedAlloc((Object*) arr, NULL);
    RETURN_PTR(arr);
}

const DalvikNativeMethod dvm_java_lang_reflect_Method[] = {
    { "getMethodModifiers", "(Ljava/lang/Class;I)I",
        Dalvik_java_lang_reflect_Method_getMethodModifiers },
    { "invokeNative",       "(Ljava/lang/Object;[Ljava/lang/Object;Ljava/lang/Class;[Ljava/lang/Class;Ljava/lang/Class;IZ)Ljava/lang/Object;",
        Dalvik_java_lang_reflect_Method_invokeNative },
    { "getDeclaredAnnotations", "(Ljava/lang/Class;I)[Ljava/lang/annotation/Annotation;",
        Dalvik_java_lang_reflect_Method_getDeclaredAnnotations },
    { "getAnnotation", "(Ljava/lang/Class;ILjava/lang/Class;)Ljava/lang/annotation/Annotation;",
        Dalvik_java_lang_reflect_Method_getAnnotation },
    { "isAnnotationPresent", "(Ljava/lang/Class;ILjava/lang/Class;)Z",
        Dalvik_java_lang_reflect_Method_isAnnotationPresent },
    { "getParameterAnnotations", "(Ljava/lang/Class;I)[[Ljava/lang/annotation/Annotation;",
        Dalvik_java_lang_reflect_Method_getParameterAnnotations },
    { "getDefaultValue",    "(Ljava/lang/Class;I)Ljava/lang/Object;",
        Dalvik_java_lang_reflect_Method_getDefaultValue },
    { "getSignatureAnnotation",  "(Ljava/lang/Class;I)[Ljava/lang/Object;",
        Dalvik_java_lang_reflect_Method_getSignatureAnnotation },
    { NULL, NULL, NULL },
};
