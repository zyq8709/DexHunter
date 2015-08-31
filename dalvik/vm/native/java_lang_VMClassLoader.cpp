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
 * java.lang.VMClassLoader
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"


/*
 * static Class defineClass(ClassLoader cl, String name,
 *     byte[] data, int offset, int len)
 *     throws ClassFormatError
 *
 * Convert an array of bytes to a Class object.
 */
static void Dalvik_java_lang_VMClassLoader_defineClass(const u4* args,
    JValue* pResult)
{
    Object* loader = (Object*) args[0];
    StringObject* nameObj = (StringObject*) args[1];
    const u1* data = (const u1*) args[2];
    int offset = args[3];
    int len = args[4];
    char* name = NULL;

    name = dvmCreateCstrFromString(nameObj);
    ALOGE("ERROR: defineClass(%p, %s, %p, %d, %d)",
        loader, name, data, offset, len);
    dvmThrowUnsupportedOperationException(
        "can't load this type of class file");

    free(name);
    RETURN_VOID();
}

/*
 * static Class defineClass(ClassLoader cl, byte[] data, int offset,
 *     int len)
 *     throws ClassFormatError
 *
 * Convert an array of bytes to a Class object. Deprecated version of
 * previous method, lacks name parameter.
 */
static void Dalvik_java_lang_VMClassLoader_defineClass2(const u4* args,
    JValue* pResult)
{
    Object* loader = (Object*) args[0];
    const u1* data = (const u1*) args[1];
    int offset = args[2];
    int len = args[3];

    ALOGE("ERROR: defineClass(%p, %p, %d, %d)",
        loader, data, offset, len);
    dvmThrowUnsupportedOperationException(
        "can't load this type of class file");

    RETURN_VOID();
}

/*
 * static Class findLoadedClass(ClassLoader cl, String name)
 */
static void Dalvik_java_lang_VMClassLoader_findLoadedClass(const u4* args,
    JValue* pResult)
{
    Object* loader = (Object*) args[0];
    StringObject* nameObj = (StringObject*) args[1];
    ClassObject* clazz = NULL;
    char* name = NULL;
    char* descriptor = NULL;

    if (nameObj == NULL) {
        dvmThrowNullPointerException("name == null");
        goto bail;
    }

    /*
     * Get a UTF-8 copy of the string, and convert dots to slashes.
     */
    name = dvmCreateCstrFromString(nameObj);
    if (name == NULL)
        goto bail;

    descriptor = dvmDotToDescriptor(name);
    if (descriptor == NULL)
        goto bail;

    clazz = dvmLookupClass(descriptor, loader, false);
    LOGVV("look: %s ldr=%p --> %p", descriptor, loader, clazz);

bail:
    free(name);
    free(descriptor);
    RETURN_PTR(clazz);
}

/*
 * private static int getBootClassPathSize()
 *
 * Get the number of entries in the boot class path.
 */
static void Dalvik_java_lang_VMClassLoader_getBootClassPathSize(const u4* args,
    JValue* pResult)
{
    int count = dvmGetBootPathSize();
    RETURN_INT(count);
}

/*
 * private static String getBootClassPathResource(String name, int index)
 *
 * Find a resource with a matching name in a boot class path entry.
 *
 * This mimics the previous VM interface, since we're sharing class libraries.
 */
static void Dalvik_java_lang_VMClassLoader_getBootClassPathResource(
    const u4* args, JValue* pResult)
{
    StringObject* nameObj = (StringObject*) args[0];
    StringObject* result;
    int idx = args[1];
    char* name;

    name = dvmCreateCstrFromString(nameObj);
    if (name == NULL)
        RETURN_PTR(NULL);

    result = dvmGetBootPathResource(name, idx);
    free(name);
    dvmReleaseTrackedAlloc((Object*)result, NULL);
    RETURN_PTR(result);
}

/*
 * static final Class getPrimitiveClass(char prim_type)
 */
static void Dalvik_java_lang_VMClassLoader_getPrimitiveClass(const u4* args,
    JValue* pResult)
{
    int primType = args[0];

    pResult->l = (Object*)dvmFindPrimitiveClass(primType);
}

/*
 * static Class loadClass(String name, boolean resolve)
 *     throws ClassNotFoundException
 *
 * Load class using bootstrap class loader.
 *
 * Return the Class object associated with the class or interface with
 * the specified name.
 *
 * "name" is in "binary name" format, e.g. "dalvik.system.Debug$1".
 */
static void Dalvik_java_lang_VMClassLoader_loadClass(const u4* args,
    JValue* pResult)
{
    StringObject* nameObj = (StringObject*) args[0];
    bool resolve = (args[1] != 0);
    ClassObject* clazz;

    clazz = dvmFindClassByName(nameObj, NULL, resolve);
    assert(clazz == NULL || dvmIsClassLinked(clazz));
    RETURN_PTR(clazz);
}

const DalvikNativeMethod dvm_java_lang_VMClassLoader[] = {
    { "defineClass",        "(Ljava/lang/ClassLoader;Ljava/lang/String;[BII)Ljava/lang/Class;",
        Dalvik_java_lang_VMClassLoader_defineClass },
    { "defineClass",        "(Ljava/lang/ClassLoader;[BII)Ljava/lang/Class;",
        Dalvik_java_lang_VMClassLoader_defineClass2 },
    { "findLoadedClass",    "(Ljava/lang/ClassLoader;Ljava/lang/String;)Ljava/lang/Class;",
        Dalvik_java_lang_VMClassLoader_findLoadedClass },
    { "getBootClassPathSize", "()I",
        Dalvik_java_lang_VMClassLoader_getBootClassPathSize },
    { "getBootClassPathResource", "(Ljava/lang/String;I)Ljava/lang/String;",
        Dalvik_java_lang_VMClassLoader_getBootClassPathResource },
    { "getPrimitiveClass",  "(C)Ljava/lang/Class;",
        Dalvik_java_lang_VMClassLoader_getPrimitiveClass },
    { "loadClass",          "(Ljava/lang/String;Z)Ljava/lang/Class;",
        Dalvik_java_lang_VMClassLoader_loadClass },
    { NULL, NULL, NULL },
};
