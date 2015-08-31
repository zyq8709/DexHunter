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
 * Internal-native initialization and some common utility functions.
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"

/*
 * Set of classes for which we provide methods.
 *
 * The last field, classNameHash, is filled in at startup.
 */
static DalvikNativeClass gDvmNativeMethodSet[] = {
    { "Ljava/lang/Object;",               dvm_java_lang_Object, 0 },
    { "Ljava/lang/Class;",                dvm_java_lang_Class, 0 },
    { "Ljava/lang/Double;",               dvm_java_lang_Double, 0 },
    { "Ljava/lang/Float;",                dvm_java_lang_Float, 0 },
    { "Ljava/lang/Math;",                 dvm_java_lang_Math, 0 },
    { "Ljava/lang/Runtime;",              dvm_java_lang_Runtime, 0 },
    { "Ljava/lang/String;",               dvm_java_lang_String, 0 },
    { "Ljava/lang/System;",               dvm_java_lang_System, 0 },
    { "Ljava/lang/Throwable;",            dvm_java_lang_Throwable, 0 },
    { "Ljava/lang/VMClassLoader;",        dvm_java_lang_VMClassLoader, 0 },
    { "Ljava/lang/VMThread;",             dvm_java_lang_VMThread, 0 },
    { "Ljava/lang/reflect/AccessibleObject;",
            dvm_java_lang_reflect_AccessibleObject, 0 },
    { "Ljava/lang/reflect/Array;",        dvm_java_lang_reflect_Array, 0 },
    { "Ljava/lang/reflect/Constructor;",
            dvm_java_lang_reflect_Constructor, 0 },
    { "Ljava/lang/reflect/Field;",        dvm_java_lang_reflect_Field, 0 },
    { "Ljava/lang/reflect/Method;",       dvm_java_lang_reflect_Method, 0 },
    { "Ljava/lang/reflect/Proxy;",        dvm_java_lang_reflect_Proxy, 0 },
    { "Ljava/util/concurrent/atomic/AtomicLong;",
            dvm_java_util_concurrent_atomic_AtomicLong, 0 },
    { "Ldalvik/bytecode/OpcodeInfo;",     dvm_dalvik_bytecode_OpcodeInfo, 0 },
    { "Ldalvik/system/VMDebug;",          dvm_dalvik_system_VMDebug, 0 },
    { "Ldalvik/system/DexFile;",          dvm_dalvik_system_DexFile, 0 },
    { "Ldalvik/system/VMRuntime;",        dvm_dalvik_system_VMRuntime, 0 },
    { "Ldalvik/system/Zygote;",           dvm_dalvik_system_Zygote, 0 },
    { "Ldalvik/system/VMStack;",          dvm_dalvik_system_VMStack, 0 },
    { "Lorg/apache/harmony/dalvik/ddmc/DdmServer;",
            dvm_org_apache_harmony_dalvik_ddmc_DdmServer, 0 },
    { "Lorg/apache/harmony/dalvik/ddmc/DdmVmInternal;",
            dvm_org_apache_harmony_dalvik_ddmc_DdmVmInternal, 0 },
    { "Lorg/apache/harmony/dalvik/NativeTestTarget;",
            dvm_org_apache_harmony_dalvik_NativeTestTarget, 0 },
    { "Lsun/misc/Unsafe;",                dvm_sun_misc_Unsafe, 0 },
    { NULL, NULL, 0 },
};


/*
 * Set up hash values on the class names.
 */
bool dvmInternalNativeStartup()
{
    DalvikNativeClass* classPtr = gDvmNativeMethodSet;

    while (classPtr->classDescriptor != NULL) {
        classPtr->classDescriptorHash =
            dvmComputeUtf8Hash(classPtr->classDescriptor);
        classPtr++;
    }

    gDvm.userDexFiles = dvmHashTableCreate(2, dvmFreeDexOrJar);
    if (gDvm.userDexFiles == NULL)
        return false;

    return true;
}

/*
 * Clean up.
 */
void dvmInternalNativeShutdown()
{
    dvmHashTableFree(gDvm.userDexFiles);
}

/*
 * Search the internal native set for a match.
 */
DalvikNativeFunc dvmLookupInternalNativeMethod(const Method* method)
{
    const char* classDescriptor = method->clazz->descriptor;
    const DalvikNativeClass* pClass;
    u4 hash;

    hash = dvmComputeUtf8Hash(classDescriptor);
    pClass = gDvmNativeMethodSet;
    while (true) {
        if (pClass->classDescriptor == NULL)
            break;
        if (pClass->classDescriptorHash == hash &&
            strcmp(pClass->classDescriptor, classDescriptor) == 0)
        {
            const DalvikNativeMethod* pMeth = pClass->methodInfo;
            while (true) {
                if (pMeth->name == NULL)
                    break;

                if (dvmCompareNameDescriptorAndMethod(pMeth->name,
                    pMeth->signature, method) == 0)
                {
                    /* match */
                    //ALOGV("+++  match on %s.%s %s at %p",
                    //    className, methodName, methodSignature, pMeth->fnPtr);
                    return pMeth->fnPtr;
                }

                pMeth++;
            }
        }

        pClass++;
    }

    return NULL;
}


/*
 * Magic "internal native" code stub, inserted into abstract method
 * definitions when a class is first loaded.  This throws the expected
 * exception so we don't have to explicitly check for it in the interpreter.
 */
void dvmAbstractMethodStub(const u4* args, JValue* pResult)
{
    ALOGD("--- called into dvmAbstractMethodStub");
    dvmThrowAbstractMethodError("abstract method not implemented");
}


/*
 * Verify that "obj" is non-null and is an instance of "clazz".
 * Used to implement reflection on fields and methods.
 *
 * Returns "false" and throws an exception if not.
 */
bool dvmVerifyObjectInClass(Object* obj, ClassObject* clazz) {
    ClassObject* exceptionClass = NULL;
    if (obj == NULL) {
        exceptionClass = gDvm.exNullPointerException;
    } else if (!dvmInstanceof(obj->clazz, clazz)) {
        exceptionClass = gDvm.exIllegalArgumentException;
    }

    if (exceptionClass == NULL) {
        return true;
    }

    std::string expectedClassName(dvmHumanReadableDescriptor(clazz->descriptor));
    std::string actualClassName(dvmHumanReadableType(obj));
    dvmThrowExceptionFmt(exceptionClass, "expected receiver of type %s, but got %s",
            expectedClassName.c_str(), actualClassName.c_str());
    return false;
}

/*
 * Find a class by name, initializing it if requested.
 */
ClassObject* dvmFindClassByName(StringObject* nameObj, Object* loader,
    bool doInit)
{
    ClassObject* clazz = NULL;
    char* name = NULL;
    char* descriptor = NULL;

    if (nameObj == NULL) {
        dvmThrowNullPointerException("name == null");
        goto bail;
    }
    name = dvmCreateCstrFromString(nameObj);

    /*
     * We need to validate and convert the name (from x.y.z to x/y/z).  This
     * is especially handy for array types, since we want to avoid
     * auto-generating bogus array classes.
     */
    if (!dexIsValidClassName(name, true)) {
        ALOGW("dvmFindClassByName rejecting '%s'", name);
        dvmThrowClassNotFoundException(name);
        goto bail;
    }

    descriptor = dvmDotToDescriptor(name);
    if (descriptor == NULL) {
        goto bail;
    }

    if (doInit)
        clazz = dvmFindClass(descriptor, loader);
    else
        clazz = dvmFindClassNoInit(descriptor, loader);

    if (clazz == NULL) {
        LOGVV("FAIL: load %s (%d)", descriptor, doInit);
        Thread* self = dvmThreadSelf();
        Object* oldExcep = dvmGetException(self);
        dvmAddTrackedAlloc(oldExcep, self);     /* don't let this be GCed */
        dvmClearException(self);
        dvmThrowChainedClassNotFoundException(name, oldExcep);
        dvmReleaseTrackedAlloc(oldExcep, self);
    } else {
        LOGVV("GOOD: load %s (%d) --> %p ldr=%p",
            descriptor, doInit, clazz, clazz->classLoader);
    }

bail:
    free(name);
    free(descriptor);
    return clazz;
}

/*
 * We insert native method stubs for abstract methods so we don't have to
 * check the access flags at the time of the method call.  This results in
 * "native abstract" methods, which can't exist.  If we see the "abstract"
 * flag set, clear the "native" flag.
 *
 * We also move the DECLARED_SYNCHRONIZED flag into the SYNCHRONIZED
 * position, because the callers of this function are trying to convey
 * the "traditional" meaning of the flags to their callers.
 */
u4 dvmFixMethodFlags(u4 flags)
{
    if ((flags & ACC_ABSTRACT) != 0) {
        flags &= ~ACC_NATIVE;
    }

    flags &= ~ACC_SYNCHRONIZED;

    if ((flags & ACC_DECLARED_SYNCHRONIZED) != 0) {
        flags |= ACC_SYNCHRONIZED;
    }

    return flags & JAVA_FLAGS_MASK;
}
