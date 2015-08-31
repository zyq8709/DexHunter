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
 * Support for -Xcheck:jni (the "careful" version of the JNI interfaces).
 *
 * We want to verify types, make sure class and field IDs are valid, and
 * ensure that JNI's semantic expectations are being met.  JNI seems to
 * be relatively lax when it comes to requirements for permission checks,
 * e.g. access to private methods is generally allowed from anywhere.
 */

#include "Dalvik.h"
#include "JniInternal.h"

#include <sys/mman.h>
#include <zlib.h>

/*
 * Abort if we are configured to bail out on JNI warnings.
 */
static void abortMaybe() {
    if (!gDvmJni.warnOnly) {
        dvmDumpThread(dvmThreadSelf(), false);
        dvmAbort();
    }
}

/*
 * ===========================================================================
 *      JNI call bridge wrapper
 * ===========================================================================
 */

/*
 * Check the result of a native method call that returns an object reference.
 *
 * The primary goal here is to verify that native code is returning the
 * correct type of object.  If it's declared to return a String but actually
 * returns a byte array, things will fail in strange ways later on.
 *
 * This can be a fairly expensive operation, since we have to look up the
 * return type class by name in method->clazz' class loader.  We take a
 * shortcut here and allow the call to succeed if the descriptor strings
 * match.  This will allow some false-positives when a class is redefined
 * by a class loader, but that's rare enough that it doesn't seem worth
 * testing for.
 *
 * At this point, pResult->l has already been converted to an object pointer.
 */
static void checkCallResultCommon(const u4* args, const JValue* pResult,
        const Method* method, Thread* self)
{
    assert(pResult->l != NULL);
    const Object* resultObj = (const Object*) pResult->l;

    if (resultObj == kInvalidIndirectRefObject) {
        ALOGW("JNI WARNING: invalid reference returned from native code");
        const Method* method = dvmGetCurrentJNIMethod();
        char* desc = dexProtoCopyMethodDescriptor(&method->prototype);
        ALOGW("             in %s.%s:%s", method->clazz->descriptor, method->name, desc);
        free(desc);
        abortMaybe();
        return;
    }

    ClassObject* objClazz = resultObj->clazz;

    /*
     * Make sure that pResult->l is an instance of the type this
     * method was expected to return.
     */
    const char* declType = dexProtoGetReturnType(&method->prototype);
    const char* objType = objClazz->descriptor;
    if (strcmp(declType, objType) == 0) {
        /* names match; ignore class loader issues and allow it */
        ALOGV("Check %s.%s: %s io %s (FAST-OK)",
            method->clazz->descriptor, method->name, objType, declType);
    } else {
        /*
         * Names didn't match.  We need to resolve declType in the context
         * of method->clazz->classLoader, and compare the class objects
         * for equality.
         *
         * Since we're returning an instance of declType, it's safe to
         * assume that it has been loaded and initialized (or, for the case
         * of an array, generated).  However, the current class loader may
         * not be listed as an initiating loader, so we can't just look for
         * it in the loaded-classes list.
         */
        ClassObject* declClazz = dvmFindClassNoInit(declType, method->clazz->classLoader);
        if (declClazz == NULL) {
            ALOGW("JNI WARNING: method declared to return '%s' returned '%s'",
                declType, objType);
            ALOGW("             failed in %s.%s ('%s' not found)",
                method->clazz->descriptor, method->name, declType);
            abortMaybe();
            return;
        }
        if (!dvmInstanceof(objClazz, declClazz)) {
            ALOGW("JNI WARNING: method declared to return '%s' returned '%s'",
                declType, objType);
            ALOGW("             failed in %s.%s",
                method->clazz->descriptor, method->name);
            abortMaybe();
            return;
        } else {
            ALOGV("Check %s.%s: %s io %s (SLOW-OK)",
                method->clazz->descriptor, method->name, objType, declType);
        }
    }
}

/*
 * Determine if we need to check the return type coming out of the call.
 *
 * (We don't simply do this at the top of checkCallResultCommon() because
 * this is on the critical path for native method calls.)
 */
static inline bool callNeedsCheck(const u4* args, JValue* pResult,
    const Method* method, Thread* self)
{
    return (method->shorty[0] == 'L' && !dvmCheckException(self) && pResult->l != NULL);
}

/*
 * Check a call into native code.
 */
void dvmCheckCallJNIMethod(const u4* args, JValue* pResult,
    const Method* method, Thread* self)
{
    dvmCallJNIMethod(args, pResult, method, self);
    if (callNeedsCheck(args, pResult, method, self)) {
        checkCallResultCommon(args, pResult, method, self);
    }
}

/*
 * ===========================================================================
 *      JNI function helpers
 * ===========================================================================
 */

static inline const JNINativeInterface* baseEnv(JNIEnv* env) {
    return ((JNIEnvExt*) env)->baseFuncTable;
}

static inline const JNIInvokeInterface* baseVm(JavaVM* vm) {
    return ((JavaVMExt*) vm)->baseFuncTable;
}

class ScopedCheckJniThreadState {
public:
    explicit ScopedCheckJniThreadState(JNIEnv* env) {
        dvmChangeStatus(NULL, THREAD_RUNNING);
    }

    ~ScopedCheckJniThreadState() {
        dvmChangeStatus(NULL, THREAD_NATIVE);
    }

private:
    // Disallow copy and assignment.
    ScopedCheckJniThreadState(const ScopedCheckJniThreadState&);
    void operator=(const ScopedCheckJniThreadState&);
};

/*
 * Flags passed into ScopedCheck.
 */
#define kFlag_Default       0x0000

#define kFlag_CritBad       0x0000      /* calling while in critical is bad */
#define kFlag_CritOkay      0x0001      /* ...okay */
#define kFlag_CritGet       0x0002      /* this is a critical "get" */
#define kFlag_CritRelease   0x0003      /* this is a critical "release" */
#define kFlag_CritMask      0x0003      /* bit mask to get "crit" value */

#define kFlag_ExcepBad      0x0000      /* raised exceptions are bad */
#define kFlag_ExcepOkay     0x0004      /* ...okay */

#define kFlag_Release       0x0010      /* are we in a non-critical release function? */
#define kFlag_NullableUtf   0x0020      /* are our UTF parameters nullable? */

#define kFlag_Invocation    0x8000      /* Part of the invocation interface (JavaVM*) */

static const char* indirectRefKindName(IndirectRef iref)
{
    return indirectRefKindToString(indirectRefKind(iref));
}

class ScopedCheck {
public:
    // For JNIEnv* functions.
    explicit ScopedCheck(JNIEnv* env, int flags, const char* functionName) {
        init(env, flags, functionName, true);
        checkThread(flags);
    }

    // For JavaVM* functions.
    explicit ScopedCheck(bool hasMethod, const char* functionName) {
        init(NULL, kFlag_Invocation, functionName, hasMethod);
    }

    /*
     * In some circumstances the VM will screen class names, but it doesn't
     * for class lookup.  When things get bounced through a class loader, they
     * can actually get normalized a couple of times; as a result, passing in
     * a class name like "java.lang.Thread" instead of "java/lang/Thread" will
     * work in some circumstances.
     *
     * This is incorrect and could cause strange behavior or compatibility
     * problems, so we want to screen that out here.
     *
     * We expect "fully-qualified" class names, like "java/lang/Thread" or
     * "[Ljava/lang/Object;".
     */
    void checkClassName(const char* className) {
        if (!dexIsValidClassName(className, false)) {
            ALOGW("JNI WARNING: illegal class name '%s' (%s)", className, mFunctionName);
            ALOGW("             (should be formed like 'dalvik/system/DexFile')");
            ALOGW("             or '[Ldalvik/system/DexFile;' or '[[B')");
            abortMaybe();
        }
    }

    void checkFieldTypeForGet(jfieldID fid, const char* expectedSignature, bool isStatic) {
        if (fid == NULL) {
            ALOGW("JNI WARNING: null jfieldID (%s)", mFunctionName);
            showLocation();
            abortMaybe();
        }

        bool printWarn = false;
        Field* field = (Field*) fid;
        const char* actualSignature = field->signature;
        if (*expectedSignature == 'L') {
            // 'actualSignature' has the exact type.
            // We just know we're expecting some kind of reference.
            if (*actualSignature != 'L' && *actualSignature != '[') {
                printWarn = true;
            }
        } else if (*actualSignature != *expectedSignature) {
            printWarn = true;
        }

        if (!printWarn && isStatic && !dvmIsStaticField(field)) {
            if (isStatic) {
                ALOGW("JNI WARNING: accessing non-static field %s as static", field->name);
            } else {
                ALOGW("JNI WARNING: accessing static field %s as non-static", field->name);
            }
            printWarn = true;
        }

        if (printWarn) {
            ALOGW("JNI WARNING: %s for field '%s' of expected type %s, got %s",
                    mFunctionName, field->name, expectedSignature, actualSignature);
            showLocation();
            abortMaybe();
        }
    }

    /*
     * Verify that the field is of the appropriate type.  If the field has an
     * object type, "jobj" is the object we're trying to assign into it.
     *
     * Works for both static and instance fields.
     */
    void checkFieldTypeForSet(jobject jobj, jfieldID fieldID, PrimitiveType prim, bool isStatic) {
        if (fieldID == NULL) {
            ALOGW("JNI WARNING: null jfieldID (%s)", mFunctionName);
            showLocation();
            abortMaybe();
        }

        bool printWarn = false;
        Field* field = (Field*) fieldID;
        if ((field->signature[0] == 'L' || field->signature[0] == '[') && jobj != NULL) {
            ScopedCheckJniThreadState ts(mEnv);
            Object* obj = dvmDecodeIndirectRef(self(), jobj);
            /*
             * If jobj is a weak global ref whose referent has been cleared,
             * obj will be NULL.  Otherwise, obj should always be non-NULL
             * and valid.
             */
            if (obj != NULL && !dvmIsHeapAddress(obj)) {
                ALOGW("JNI WARNING: field operation (%s) on invalid %s reference (%p)",
                      mFunctionName, indirectRefKindName(jobj), jobj);
                printWarn = true;
            } else {
                ClassObject* fieldClass = dvmFindLoadedClass(field->signature);
                ClassObject* objClass = obj->clazz;

                assert(fieldClass != NULL);
                assert(objClass != NULL);

                if (!dvmInstanceof(objClass, fieldClass)) {
                    ALOGW("JNI WARNING: %s for field '%s' expected type %s, got %s",
                          mFunctionName, field->name, field->signature, objClass->descriptor);
                    printWarn = true;
                }
            }
        } else if (dexGetPrimitiveTypeFromDescriptorChar(field->signature[0]) != prim) {
            ALOGW("JNI WARNING: %s for field '%s' expected type %s, got %s",
                    mFunctionName, field->name, field->signature, primitiveTypeToName(prim));
            printWarn = true;
        } else if (isStatic && !dvmIsStaticField(field)) {
            if (isStatic) {
                ALOGW("JNI WARNING: %s for non-static field '%s'", mFunctionName, field->name);
            } else {
                ALOGW("JNI WARNING: %s for static field '%s'", mFunctionName, field->name);
            }
            printWarn = true;
        }

        if (printWarn) {
            showLocation();
            abortMaybe();
        }
    }

    /*
     * Verify that this instance field ID is valid for this object.
     *
     * Assumes "jobj" has already been validated.
     */
    void checkInstanceFieldID(jobject jobj, jfieldID fieldID) {
        ScopedCheckJniThreadState ts(mEnv);

        Object* obj = dvmDecodeIndirectRef(self(), jobj);
        if (!dvmIsHeapAddress(obj)) {
            ALOGW("JNI ERROR: %s on invalid reference (%p)", mFunctionName, jobj);
            dvmAbort();
        }

        /*
         * Check this class and all of its superclasses for a matching field.
         * Don't need to scan interfaces.
         */
        ClassObject* clazz = obj->clazz;
        while (clazz != NULL) {
            if ((InstField*) fieldID >= clazz->ifields &&
                    (InstField*) fieldID < clazz->ifields + clazz->ifieldCount) {
                return;
            }

            clazz = clazz->super;
        }

        ALOGW("JNI WARNING: instance jfieldID %p not valid for class %s (%s)",
              fieldID, obj->clazz->descriptor, mFunctionName);
        showLocation();
        abortMaybe();
    }

    /*
     * Verify that the pointer value is non-NULL.
     */
    void checkNonNull(const void* ptr) {
        if (ptr == NULL) {
            ALOGW("JNI WARNING: invalid null pointer (%s)", mFunctionName);
            abortMaybe();
        }
    }

    /*
     * Verify that the method's return type matches the type of call.
     * 'expectedType' will be "L" for all objects, including arrays.
     */
    void checkSig(jmethodID methodID, const char* expectedType, bool isStatic) {
        const Method* method = (const Method*) methodID;
        bool printWarn = false;

        if (*expectedType != method->shorty[0]) {
            ALOGW("JNI WARNING: %s expected return type '%s'", mFunctionName, expectedType);
            printWarn = true;
        } else if (isStatic && !dvmIsStaticMethod(method)) {
            if (isStatic) {
                ALOGW("JNI WARNING: calling non-static method with static call %s", mFunctionName);
            } else {
                ALOGW("JNI WARNING: calling static method with non-static call %s", mFunctionName);
            }
            printWarn = true;
        }

        if (printWarn) {
            char* desc = dexProtoCopyMethodDescriptor(&method->prototype);
            ALOGW("             calling %s.%s %s", method->clazz->descriptor, method->name, desc);
            free(desc);
            showLocation();
            abortMaybe();
        }
    }

    /*
     * Verify that this static field ID is valid for this class.
     *
     * Assumes "jclazz" has already been validated.
     */
    void checkStaticFieldID(jclass jclazz, jfieldID fieldID) {
        ScopedCheckJniThreadState ts(mEnv);
        ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(self(), jclazz);
        StaticField* base = &clazz->sfields[0];
        int fieldCount = clazz->sfieldCount;
        if ((StaticField*) fieldID < base || (StaticField*) fieldID >= base + fieldCount) {
            ALOGW("JNI WARNING: static fieldID %p not valid for class %s (%s)",
                  fieldID, clazz->descriptor, mFunctionName);
            ALOGW("             base=%p count=%d", base, fieldCount);
            showLocation();
            abortMaybe();
        }
    }

    /*
     * Verify that "methodID" is appropriate for "clazz".
     *
     * A mismatch isn't dangerous, because the jmethodID defines the class.  In
     * fact, jclazz is unused in the implementation.  It's best if we don't
     * allow bad code in the system though.
     *
     * Instances of "jclazz" must be instances of the method's declaring class.
     */
    void checkStaticMethod(jclass jclazz, jmethodID methodID) {
        ScopedCheckJniThreadState ts(mEnv);

        ClassObject* clazz = (ClassObject*) dvmDecodeIndirectRef(self(), jclazz);
        const Method* method = (const Method*) methodID;

        if (!dvmInstanceof(clazz, method->clazz)) {
            ALOGW("JNI WARNING: can't call static %s.%s on class %s (%s)",
                  method->clazz->descriptor, method->name, clazz->descriptor, mFunctionName);
            showLocation();
            // no abort?
        }
    }

    /*
     * Verify that "methodID" is appropriate for "jobj".
     *
     * Make sure the object is an instance of the method's declaring class.
     * (Note the methodID might point to a declaration in an interface; this
     * will be handled automatically by the instanceof check.)
     */
    void checkVirtualMethod(jobject jobj, jmethodID methodID) {
        ScopedCheckJniThreadState ts(mEnv);

        Object* obj = dvmDecodeIndirectRef(self(), jobj);
        const Method* method = (const Method*) methodID;

        if (!dvmInstanceof(obj->clazz, method->clazz)) {
            ALOGW("JNI WARNING: can't call %s.%s on instance of %s (%s)",
                  method->clazz->descriptor, method->name, obj->clazz->descriptor, mFunctionName);
            showLocation();
            abortMaybe();
        }
    }

    /**
     * The format string is a sequence of the following characters,
     * and must be followed by arguments of the corresponding types
     * in the same order.
     *
     * Java primitive types:
     * B - jbyte
     * C - jchar
     * D - jdouble
     * F - jfloat
     * I - jint
     * J - jlong
     * S - jshort
     * Z - jboolean (shown as true and false)
     * V - void
     *
     * Java reference types:
     * L - jobject
     * a - jarray
     * c - jclass
     * s - jstring
     *
     * JNI types:
     * b - jboolean (shown as JNI_TRUE and JNI_FALSE)
     * f - jfieldID
     * m - jmethodID
     * p - void*
     * r - jint (for release mode arguments)
     * t - thread args (for AttachCurrentThread)
     * u - const char* (modified UTF-8)
     * z - jsize (for lengths; use i if negative values are okay)
     * v - JavaVM*
     * E - JNIEnv*
     * . - no argument; just print "..." (used for varargs JNI calls)
     *
     * Use the kFlag_NullableUtf flag where 'u' field(s) are nullable.
     */
    void check(bool entry, const char* fmt0, ...) {
        va_list ap;

        bool shouldTrace = false;
        const Method* method = NULL;
        if ((gDvm.jniTrace || gDvmJni.logThirdPartyJni) && mHasMethod) {
            // We need to guard some of the invocation interface's calls: a bad caller might
            // use DetachCurrentThread or GetEnv on a thread that's not yet attached.
            if ((mFlags & kFlag_Invocation) == 0 || dvmThreadSelf() != NULL) {
                method = dvmGetCurrentJNIMethod();
            }
        }
        if (method != NULL) {
            // If both "-Xcheck:jni" and "-Xjnitrace:" are enabled, we print trace messages
            // when a native method that matches the Xjnitrace argument calls a JNI function
            // such as NewByteArray.
            if (gDvm.jniTrace && strstr(method->clazz->descriptor, gDvm.jniTrace) != NULL) {
                shouldTrace = true;
            }
            // If -Xjniopts:logThirdPartyJni is on, we want to log any JNI function calls
            // made by a third-party native method.
            if (gDvmJni.logThirdPartyJni) {
                shouldTrace |= method->shouldTrace;
            }
        }

        if (shouldTrace) {
            va_start(ap, fmt0);
            std::string msg;
            for (const char* fmt = fmt0; *fmt;) {
                char ch = *fmt++;
                if (ch == 'B') { // jbyte
                    jbyte b = va_arg(ap, int);
                    if (b >= 0 && b < 10) {
                        StringAppendF(&msg, "%d", b);
                    } else {
                        StringAppendF(&msg, "%#x (%d)", b, b);
                    }
                } else if (ch == 'C') { // jchar
                    jchar c = va_arg(ap, int);
                    if (c < 0x7f && c >= ' ') {
                        StringAppendF(&msg, "U+%x ('%c')", c, c);
                    } else {
                        StringAppendF(&msg, "U+%x", c);
                    }
                } else if (ch == 'F' || ch == 'D') { // jfloat, jdouble
                    StringAppendF(&msg, "%g", va_arg(ap, double));
                } else if (ch == 'I' || ch == 'S') { // jint, jshort
                    StringAppendF(&msg, "%d", va_arg(ap, int));
                } else if (ch == 'J') { // jlong
                    StringAppendF(&msg, "%lld", va_arg(ap, jlong));
                } else if (ch == 'Z') { // jboolean
                    StringAppendF(&msg, "%s", va_arg(ap, int) ? "true" : "false");
                } else if (ch == 'V') { // void
                    msg += "void";
                } else if (ch == 'v') { // JavaVM*
                    JavaVM* vm = va_arg(ap, JavaVM*);
                    StringAppendF(&msg, "(JavaVM*)%p", vm);
                } else if (ch == 'E') { // JNIEnv*
                    JNIEnv* env = va_arg(ap, JNIEnv*);
                    StringAppendF(&msg, "(JNIEnv*)%p", env);
                } else if (ch == 'L' || ch == 'a' || ch == 's') { // jobject, jarray, jstring
                    // For logging purposes, these are identical.
                    jobject o = va_arg(ap, jobject);
                    if (o == NULL) {
                        msg += "NULL";
                    } else {
                        StringAppendF(&msg, "%p", o);
                    }
                } else if (ch == 'b') { // jboolean (JNI-style)
                    jboolean b = va_arg(ap, int);
                    msg += (b ? "JNI_TRUE" : "JNI_FALSE");
                } else if (ch == 'c') { // jclass
                    jclass jc = va_arg(ap, jclass);
                    Object* c = dvmDecodeIndirectRef(self(), jc);
                    if (c == NULL) {
                        msg += "NULL";
                    } else if (c == kInvalidIndirectRefObject || !dvmIsHeapAddress(c)) {
                        StringAppendF(&msg, "%p(INVALID)", jc);
                    } else {
                        std::string className(dvmHumanReadableType(c));
                        StringAppendF(&msg, "%s", className.c_str());
                        if (!entry) {
                            StringAppendF(&msg, " (%p)", jc);
                        }
                    }
                } else if (ch == 'f') { // jfieldID
                    jfieldID fid = va_arg(ap, jfieldID);
                    std::string name(dvmHumanReadableField((Field*) fid));
                    StringAppendF(&msg, "%s", name.c_str());
                    if (!entry) {
                        StringAppendF(&msg, " (%p)", fid);
                    }
                } else if (ch == 'z') { // non-negative jsize
                    // You might expect jsize to be size_t, but it's not; it's the same as jint.
                    // We only treat this specially so we can do the non-negative check.
                    // TODO: maybe this wasn't worth it?
                    jint i = va_arg(ap, jint);
                    StringAppendF(&msg, "%d", i);
                } else if (ch == 'm') { // jmethodID
                    jmethodID mid = va_arg(ap, jmethodID);
                    std::string name(dvmHumanReadableMethod((Method*) mid, true));
                    StringAppendF(&msg, "%s", name.c_str());
                    if (!entry) {
                        StringAppendF(&msg, " (%p)", mid);
                    }
                } else if (ch == 'p' || ch == 't') { // void* ("pointer" or "thread args")
                    void* p = va_arg(ap, void*);
                    if (p == NULL) {
                        msg += "NULL";
                    } else {
                        StringAppendF(&msg, "(void*) %p", p);
                    }
                } else if (ch == 'r') { // jint (release mode)
                    jint releaseMode = va_arg(ap, jint);
                    if (releaseMode == 0) {
                        msg += "0";
                    } else if (releaseMode == JNI_ABORT) {
                        msg += "JNI_ABORT";
                    } else if (releaseMode == JNI_COMMIT) {
                        msg += "JNI_COMMIT";
                    } else {
                        StringAppendF(&msg, "invalid release mode %d", releaseMode);
                    }
                } else if (ch == 'u') { // const char* (modified UTF-8)
                    const char* utf = va_arg(ap, const char*);
                    if (utf == NULL) {
                        msg += "NULL";
                    } else {
                        StringAppendF(&msg, "\"%s\"", utf);
                    }
                } else if (ch == '.') {
                    msg += "...";
                } else {
                    ALOGE("unknown trace format specifier %c", ch);
                    dvmAbort();
                }
                if (*fmt) {
                    StringAppendF(&msg, ", ");
                }
            }
            va_end(ap);

            if (entry) {
                if (mHasMethod) {
                    std::string methodName(dvmHumanReadableMethod(method, false));
                    ALOGI("JNI: %s -> %s(%s)", methodName.c_str(), mFunctionName, msg.c_str());
                    mIndent = methodName.size() + 1;
                } else {
                    ALOGI("JNI: -> %s(%s)", mFunctionName, msg.c_str());
                    mIndent = 0;
                }
            } else {
                ALOGI("JNI: %*s<- %s returned %s", mIndent, "", mFunctionName, msg.c_str());
            }
        }

        // We always do the thorough checks on entry, and never on exit...
        if (entry) {
            va_start(ap, fmt0);
            for (const char* fmt = fmt0; *fmt; ++fmt) {
                char ch = *fmt;
                if (ch == 'a') {
                    checkArray(va_arg(ap, jarray));
                } else if (ch == 'c') {
                    checkClass(va_arg(ap, jclass));
                } else if (ch == 'L') {
                    checkObject(va_arg(ap, jobject));
                } else if (ch == 'r') {
                    checkReleaseMode(va_arg(ap, jint));
                } else if (ch == 's') {
                    checkString(va_arg(ap, jstring));
                } else if (ch == 't') {
                    checkThreadArgs(va_arg(ap, void*));
                } else if (ch == 'u') {
                    if ((mFlags & kFlag_Release) != 0) {
                        checkNonNull(va_arg(ap, const char*));
                    } else {
                        bool nullable = ((mFlags & kFlag_NullableUtf) != 0);
                        checkUtfString(va_arg(ap, const char*), nullable);
                    }
                } else if (ch == 'z') {
                    checkLengthPositive(va_arg(ap, jsize));
                } else if (strchr("BCISZbfmpEv", ch) != NULL) {
                    va_arg(ap, int); // Skip this argument.
                } else if (ch == 'D' || ch == 'F') {
                    va_arg(ap, double); // Skip this argument.
                } else if (ch == 'J') {
                    va_arg(ap, long); // Skip this argument.
                } else if (ch == '.') {
                } else {
                    ALOGE("unknown check format specifier %c", ch);
                    dvmAbort();
                }
            }
            va_end(ap);
        }
    }

    // Only safe after checkThread returns.
    Thread* self() {
        return ((JNIEnvExt*) mEnv)->self;
    }

private:
    JNIEnv* mEnv;
    const char* mFunctionName;
    int mFlags;
    bool mHasMethod;
    size_t mIndent;

    void init(JNIEnv* env, int flags, const char* functionName, bool hasMethod) {
        mEnv = env;
        mFlags = flags;

        // Use +6 to drop the leading "Check_"...
        mFunctionName = functionName + 6;

        // Set "hasMethod" to true if we have a valid thread with a method pointer.
        // We won't have one before attaching a thread, after detaching a thread, or
        // after destroying the VM.
        mHasMethod = hasMethod;
    }

    /*
     * Verify that "array" is non-NULL and points to an Array object.
     *
     * Since we're dealing with objects, switch to "running" mode.
     */
    void checkArray(jarray jarr) {
        if (jarr == NULL) {
            ALOGW("JNI WARNING: %s received null array", mFunctionName);
            showLocation();
            abortMaybe();
            return;
        }

        ScopedCheckJniThreadState ts(mEnv);
        bool printWarn = false;

        Object* obj = dvmDecodeIndirectRef(self(), jarr);
        if (!dvmIsHeapAddress(obj)) {
            ALOGW("JNI WARNING: %s: jarray is an invalid %s reference (%p)",
                  mFunctionName, indirectRefKindName(jarr), jarr);
            printWarn = true;
        } else if (obj->clazz->descriptor[0] != '[') {
            ALOGW("JNI WARNING: %s: jarray arg has wrong type (expected array, got %s)",
                  mFunctionName, obj->clazz->descriptor);
            printWarn = true;
        }

        if (printWarn) {
            showLocation();
            abortMaybe();
        }
    }

    void checkClass(jclass c) {
        checkInstance(c, gDvm.classJavaLangClass, "jclass");
    }

    void checkLengthPositive(jsize length) {
        if (length < 0) {
            ALOGW("JNI WARNING: negative jsize (%s)", mFunctionName);
            abortMaybe();
        }
    }

    /*
     * Verify that "jobj" is a valid object, and that it's an object that JNI
     * is allowed to know about.  We allow NULL references.
     *
     * Switches to "running" mode before performing checks.
     */
    void checkObject(jobject jobj) {
        if (jobj == NULL) {
            return;
        }

        ScopedCheckJniThreadState ts(mEnv);

        bool printWarn = false;
        if (dvmGetJNIRefType(self(), jobj) == JNIInvalidRefType) {
            ALOGW("JNI WARNING: %p is not a valid JNI reference (%s)", jobj, mFunctionName);
            printWarn = true;
        } else {
            Object* obj = dvmDecodeIndirectRef(self(), jobj);
            if (obj == kInvalidIndirectRefObject) {
                ALOGW("JNI WARNING: native code passing in invalid reference %p (%s)",
                      jobj, mFunctionName);
                printWarn = true;
            } else if (obj != NULL && !dvmIsHeapAddress(obj)) {
                // TODO: when we remove workAroundAppJniBugs, this should be impossible.
                ALOGW("JNI WARNING: native code passing in reference to invalid object %p %p (%s)",
                        jobj, obj, mFunctionName);
                printWarn = true;
            }
        }

        if (printWarn) {
            showLocation();
            abortMaybe();
        }
    }

    /*
     * Verify that the "mode" argument passed to a primitive array Release
     * function is one of the valid values.
     */
    void checkReleaseMode(jint mode) {
        if (mode != 0 && mode != JNI_COMMIT && mode != JNI_ABORT) {
            ALOGW("JNI WARNING: bad value for mode (%d) (%s)", mode, mFunctionName);
            abortMaybe();
        }
    }

    void checkString(jstring s) {
        checkInstance(s, gDvm.classJavaLangString, "jstring");
    }

    void checkThreadArgs(void* thread_args) {
        JavaVMAttachArgs* args = static_cast<JavaVMAttachArgs*>(thread_args);
        if (args != NULL && args->version < JNI_VERSION_1_2) {
            ALOGW("JNI WARNING: bad value for JNI version (%d) (%s)", args->version, mFunctionName);
            abortMaybe();
        }
    }

    void checkThread(int flags) {
        // Get the *correct* JNIEnv by going through our TLS pointer.
        JNIEnvExt* threadEnv = dvmGetJNIEnvForThread();

        /*
         * Verify that the current thread is (a) attached and (b) associated with
         * this particular instance of JNIEnv.
         */
        bool printWarn = false;
        if (threadEnv == NULL) {
            ALOGE("JNI ERROR: non-VM thread making JNI call (%s)", mFunctionName);
            // don't set printWarn -- it'll try to call showLocation()
            dvmAbort();
        } else if ((JNIEnvExt*) mEnv != threadEnv) {
            if (dvmThreadSelf()->threadId != threadEnv->envThreadId) {
                ALOGE("JNI: threadEnv != thread->env? (%s)", mFunctionName);
                dvmAbort();
            }

            ALOGW("JNI WARNING: threadid=%d using env from threadid=%d (%s)",
                  threadEnv->envThreadId, ((JNIEnvExt*) mEnv)->envThreadId, mFunctionName);
            printWarn = true;

            // If we're keeping broken code limping along, we need to suppress the abort...
            if (gDvmJni.workAroundAppJniBugs) {
                printWarn = false;
            }

            /* this is a bad idea -- need to throw as we exit, or abort func */
            //dvmThrowRuntimeException("invalid use of JNI env ptr");
        } else if (((JNIEnvExt*) mEnv)->self != dvmThreadSelf()) {
            /* correct JNIEnv*; make sure the "self" pointer is correct */
            ALOGE("JNI ERROR: env->self != thread-self (%p vs. %p) (%s)",
                  ((JNIEnvExt*) mEnv)->self, dvmThreadSelf(), mFunctionName);
            dvmAbort();
        }

        /*
         * Verify that, if this thread previously made a critical "get" call, we
         * do the corresponding "release" call before we try anything else.
         */
        switch (flags & kFlag_CritMask) {
        case kFlag_CritOkay:    // okay to call this method
            break;
        case kFlag_CritBad:     // not okay to call
            if (threadEnv->critical) {
                ALOGW("JNI WARNING: threadid=%d using JNI after critical get (%s)",
                      threadEnv->envThreadId, mFunctionName);
                printWarn = true;
            }
            break;
        case kFlag_CritGet:     // this is a "get" call
            /* don't check here; we allow nested gets */
            threadEnv->critical++;
            break;
        case kFlag_CritRelease: // this is a "release" call
            threadEnv->critical--;
            if (threadEnv->critical < 0) {
                ALOGW("JNI WARNING: threadid=%d called too many critical releases (%s)",
                      threadEnv->envThreadId, mFunctionName);
                printWarn = true;
            }
            break;
        default:
            assert(false);
        }

        /*
         * Verify that, if an exception has been raised, the native code doesn't
         * make any JNI calls other than the Exception* methods.
         */
        bool printException = false;
        if ((flags & kFlag_ExcepOkay) == 0 && dvmCheckException(dvmThreadSelf())) {
            ALOGW("JNI WARNING: JNI function %s called with exception pending", mFunctionName);
            printWarn = true;
            printException = true;
        }

        if (printWarn) {
            showLocation();
        }
        if (printException) {
            ALOGW("Pending exception is:");
            dvmLogExceptionStackTrace();
        }
        if (printWarn) {
            abortMaybe();
        }
    }

    /*
     * Verify that "bytes" points to valid "modified UTF-8" data.
     */
    void checkUtfString(const char* bytes, bool nullable) {
        if (bytes == NULL) {
            if (!nullable) {
                ALOGW("JNI WARNING: non-nullable const char* was NULL (%s)", mFunctionName);
                showLocation();
                abortMaybe();
            }
            return;
        }

        const char* errorKind = NULL;
        u1 utf8 = checkUtfBytes(bytes, &errorKind);
        if (errorKind != NULL) {
            ALOGW("JNI WARNING: %s input is not valid Modified UTF-8: illegal %s byte %#x",
                  mFunctionName, errorKind, utf8);
            ALOGW("             string: '%s'", bytes);
            showLocation();
            abortMaybe();
        }
    }

    /*
     * Verify that "jobj" is a valid non-NULL object reference, and points to
     * an instance of expectedClass.
     *
     * Because we're looking at an object on the GC heap, we have to switch
     * to "running" mode before doing the checks.
     */
    void checkInstance(jobject jobj, ClassObject* expectedClass, const char* argName) {
        if (jobj == NULL) {
            ALOGW("JNI WARNING: received null %s (%s)", argName, mFunctionName);
            showLocation();
            abortMaybe();
            return;
        }

        ScopedCheckJniThreadState ts(mEnv);
        bool printWarn = false;

        Object* obj = dvmDecodeIndirectRef(self(), jobj);
        if (!dvmIsHeapAddress(obj)) {
            ALOGW("JNI WARNING: %s is an invalid %s reference (%p) (%s)",
                  argName, indirectRefKindName(jobj), jobj, mFunctionName);
            printWarn = true;
        } else if (obj->clazz != expectedClass) {
            ALOGW("JNI WARNING: %s arg has wrong type (expected %s, got %s) (%s)",
                  argName, expectedClass->descriptor, obj->clazz->descriptor, mFunctionName);
            printWarn = true;
        }

        if (printWarn) {
            showLocation();
            abortMaybe();
        }
    }

    static u1 checkUtfBytes(const char* bytes, const char** errorKind) {
        while (*bytes != '\0') {
            u1 utf8 = *(bytes++);
            // Switch on the high four bits.
            switch (utf8 >> 4) {
            case 0x00:
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x06:
            case 0x07:
                // Bit pattern 0xxx. No need for any extra bytes.
                break;
            case 0x08:
            case 0x09:
            case 0x0a:
            case 0x0b:
            case 0x0f:
                /*
                 * Bit pattern 10xx or 1111, which are illegal start bytes.
                 * Note: 1111 is valid for normal UTF-8, but not the
                 * modified UTF-8 used here.
                 */
                *errorKind = "start";
                return utf8;
            case 0x0e:
                // Bit pattern 1110, so there are two additional bytes.
                utf8 = *(bytes++);
                if ((utf8 & 0xc0) != 0x80) {
                    *errorKind = "continuation";
                    return utf8;
                }
                // Fall through to take care of the final byte.
            case 0x0c:
            case 0x0d:
                // Bit pattern 110x, so there is one additional byte.
                utf8 = *(bytes++);
                if ((utf8 & 0xc0) != 0x80) {
                    *errorKind = "continuation";
                    return utf8;
                }
                break;
            }
        }
        return 0;
    }

    /**
     * Returns a human-readable name for the given primitive type.
     */
    static const char* primitiveTypeToName(PrimitiveType primType) {
        switch (primType) {
        case PRIM_VOID:    return "void";
        case PRIM_BOOLEAN: return "boolean";
        case PRIM_BYTE:    return "byte";
        case PRIM_SHORT:   return "short";
        case PRIM_CHAR:    return "char";
        case PRIM_INT:     return "int";
        case PRIM_LONG:    return "long";
        case PRIM_FLOAT:   return "float";
        case PRIM_DOUBLE:  return "double";
        case PRIM_NOT:     return "Object/array";
        default:           return "???";
        }
    }

    void showLocation() {
        const Method* method = dvmGetCurrentJNIMethod();
        char* desc = dexProtoCopyMethodDescriptor(&method->prototype);
        ALOGW("             in %s.%s:%s (%s)", method->clazz->descriptor, method->name, desc, mFunctionName);
        free(desc);
    }

    // Disallow copy and assignment.
    ScopedCheck(const ScopedCheck&);
    void operator=(const ScopedCheck&);
};

/*
 * ===========================================================================
 *      Guarded arrays
 * ===========================================================================
 */

#define kGuardLen       512         /* must be multiple of 2 */
#define kGuardPattern   0xd5e3      /* uncommon values; d5e3d5e3 invalid addr */
#define kGuardMagic     0xffd5aa96

/* this gets tucked in at the start of the buffer; struct size must be even */
struct GuardedCopy {
    u4          magic;
    uLong       adler;
    size_t      originalLen;
    const void* originalPtr;

    /* find the GuardedCopy given the pointer into the "live" data */
    static inline const GuardedCopy* fromData(const void* dataBuf) {
        return reinterpret_cast<const GuardedCopy*>(actualBuffer(dataBuf));
    }

    /*
     * Create an over-sized buffer to hold the contents of "buf".  Copy it in,
     * filling in the area around it with guard data.
     *
     * We use a 16-bit pattern to make a rogue memset less likely to elude us.
     */
    static void* create(const void* buf, size_t len, bool modOkay) {
        size_t newLen = actualLength(len);
        u1* newBuf = debugAlloc(newLen);

        /* fill it in with a pattern */
        u2* pat = (u2*) newBuf;
        for (size_t i = 0; i < newLen / 2; i++) {
            *pat++ = kGuardPattern;
        }

        /* copy the data in; note "len" could be zero */
        memcpy(newBuf + kGuardLen / 2, buf, len);

        /* if modification is not expected, grab a checksum */
        uLong adler = 0;
        if (!modOkay) {
            adler = adler32(0L, Z_NULL, 0);
            adler = adler32(adler, (const Bytef*)buf, len);
            *(uLong*)newBuf = adler;
        }

        GuardedCopy* pExtra = reinterpret_cast<GuardedCopy*>(newBuf);
        pExtra->magic = kGuardMagic;
        pExtra->adler = adler;
        pExtra->originalPtr = buf;
        pExtra->originalLen = len;

        return newBuf + kGuardLen / 2;
    }

    /*
     * Free up the guard buffer, scrub it, and return the original pointer.
     */
    static void* destroy(void* dataBuf) {
        const GuardedCopy* pExtra = GuardedCopy::fromData(dataBuf);
        void* originalPtr = (void*) pExtra->originalPtr;
        size_t len = pExtra->originalLen;
        debugFree(dataBuf, len);
        return originalPtr;
    }

    /*
     * Verify the guard area and, if "modOkay" is false, that the data itself
     * has not been altered.
     *
     * The caller has already checked that "dataBuf" is non-NULL.
     */
    static bool check(const void* dataBuf, bool modOkay) {
        static const u4 kMagicCmp = kGuardMagic;
        const u1* fullBuf = actualBuffer(dataBuf);
        const GuardedCopy* pExtra = GuardedCopy::fromData(dataBuf);

        /*
         * Before we do anything with "pExtra", check the magic number.  We
         * do the check with memcmp rather than "==" in case the pointer is
         * unaligned.  If it points to completely bogus memory we're going
         * to crash, but there's no easy way around that.
         */
        if (memcmp(&pExtra->magic, &kMagicCmp, 4) != 0) {
            u1 buf[4];
            memcpy(buf, &pExtra->magic, 4);
            ALOGE("JNI: guard magic does not match (found 0x%02x%02x%02x%02x) -- incorrect data pointer %p?",
                    buf[3], buf[2], buf[1], buf[0], dataBuf); /* assume little endian */
            return false;
        }

        size_t len = pExtra->originalLen;

        /* check bottom half of guard; skip over optional checksum storage */
        const u2* pat = (u2*) fullBuf;
        for (size_t i = sizeof(GuardedCopy) / 2; i < (kGuardLen / 2 - sizeof(GuardedCopy)) / 2; i++) {
            if (pat[i] != kGuardPattern) {
                ALOGE("JNI: guard pattern(1) disturbed at %p + %d", fullBuf, i*2);
                return false;
            }
        }

        int offset = kGuardLen / 2 + len;
        if (offset & 0x01) {
            /* odd byte; expected value depends on endian-ness of host */
            const u2 patSample = kGuardPattern;
            if (fullBuf[offset] != ((const u1*) &patSample)[1]) {
                ALOGE("JNI: guard pattern disturbed in odd byte after %p (+%d) 0x%02x 0x%02x",
                        fullBuf, offset, fullBuf[offset], ((const u1*) &patSample)[1]);
                return false;
            }
            offset++;
        }

        /* check top half of guard */
        pat = (u2*) (fullBuf + offset);
        for (size_t i = 0; i < kGuardLen / 4; i++) {
            if (pat[i] != kGuardPattern) {
                ALOGE("JNI: guard pattern(2) disturbed at %p + %d", fullBuf, offset + i*2);
                return false;
            }
        }

        /*
         * If modification is not expected, verify checksum.  Strictly speaking
         * this is wrong: if we told the client that we made a copy, there's no
         * reason they can't alter the buffer.
         */
        if (!modOkay) {
            uLong adler = adler32(0L, Z_NULL, 0);
            adler = adler32(adler, (const Bytef*)dataBuf, len);
            if (pExtra->adler != adler) {
                ALOGE("JNI: buffer modified (0x%08lx vs 0x%08lx) at addr %p",
                        pExtra->adler, adler, dataBuf);
                return false;
            }
        }

        return true;
    }

private:
    static u1* debugAlloc(size_t len) {
        void* result = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
        if (result == MAP_FAILED) {
            ALOGE("GuardedCopy::create mmap(%d) failed: %s", len, strerror(errno));
            dvmAbort();
        }
        return reinterpret_cast<u1*>(result);
    }

    static void debugFree(void* dataBuf, size_t len) {
        u1* fullBuf = actualBuffer(dataBuf);
        size_t totalByteCount = actualLength(len);
        // TODO: we could mprotect instead, and keep the allocation around for a while.
        // This would be even more expensive, but it might catch more errors.
        // if (mprotect(fullBuf, totalByteCount, PROT_NONE) != 0) {
        //     ALOGW("mprotect(PROT_NONE) failed: %s", strerror(errno));
        // }
        if (munmap(fullBuf, totalByteCount) != 0) {
            ALOGW("munmap failed: %s", strerror(errno));
            dvmAbort();
        }
    }

    static const u1* actualBuffer(const void* dataBuf) {
        return reinterpret_cast<const u1*>(dataBuf) - kGuardLen / 2;
    }

    static u1* actualBuffer(void* dataBuf) {
        return reinterpret_cast<u1*>(dataBuf) - kGuardLen / 2;
    }

    // Underlying length of a user allocation of 'length' bytes.
    static size_t actualLength(size_t length) {
        return (length + kGuardLen + 1) & ~0x01;
    }
};

/*
 * Return the width, in bytes, of a primitive type.
 */
static int dvmPrimitiveTypeWidth(PrimitiveType primType) {
    switch (primType) {
        case PRIM_BOOLEAN: return 1;
        case PRIM_BYTE:    return 1;
        case PRIM_SHORT:   return 2;
        case PRIM_CHAR:    return 2;
        case PRIM_INT:     return 4;
        case PRIM_LONG:    return 8;
        case PRIM_FLOAT:   return 4;
        case PRIM_DOUBLE:  return 8;
        case PRIM_VOID:
        default: {
            assert(false);
            return -1;
        }
    }
}

/*
 * Create a guarded copy of a primitive array.  Modifications to the copied
 * data are allowed.  Returns a pointer to the copied data.
 */
static void* createGuardedPACopy(JNIEnv* env, const jarray jarr, jboolean* isCopy) {
    ScopedCheckJniThreadState ts(env);

    ArrayObject* arrObj = (ArrayObject*) dvmDecodeIndirectRef(dvmThreadSelf(), jarr);
    PrimitiveType primType = arrObj->clazz->elementClass->primitiveType;
    int len = arrObj->length * dvmPrimitiveTypeWidth(primType);
    void* result = GuardedCopy::create(arrObj->contents, len, true);
    if (isCopy != NULL) {
        *isCopy = JNI_TRUE;
    }
    return result;
}

/*
 * Perform the array "release" operation, which may or may not copy data
 * back into the VM, and may or may not release the underlying storage.
 */
static void* releaseGuardedPACopy(JNIEnv* env, jarray jarr, void* dataBuf, int mode) {
    ScopedCheckJniThreadState ts(env);
    ArrayObject* arrObj = (ArrayObject*) dvmDecodeIndirectRef(dvmThreadSelf(), jarr);

    if (!GuardedCopy::check(dataBuf, true)) {
        ALOGE("JNI: failed guarded copy check in releaseGuardedPACopy");
        abortMaybe();
        return NULL;
    }

    if (mode != JNI_ABORT) {
        size_t len = GuardedCopy::fromData(dataBuf)->originalLen;
        memcpy(arrObj->contents, dataBuf, len);
    }

    u1* result = NULL;
    if (mode != JNI_COMMIT) {
        result = (u1*) GuardedCopy::destroy(dataBuf);
    } else {
        result = (u1*) (void*) GuardedCopy::fromData(dataBuf)->originalPtr;
    }

    /* pointer is to the array contents; back up to the array object */
    result -= OFFSETOF_MEMBER(ArrayObject, contents);
    return result;
}


/*
 * ===========================================================================
 *      JNI functions
 * ===========================================================================
 */

#define CHECK_JNI_ENTRY(flags, types, args...) \
    ScopedCheck sc(env, flags, __FUNCTION__); \
    sc.check(true, types, ##args)

#define CHECK_JNI_EXIT(type, exp) ({ \
    typeof (exp) _rc = (exp); \
    sc.check(false, type, _rc); \
    _rc; })
#define CHECK_JNI_EXIT_VOID() \
    sc.check(false, "V")

static jint Check_GetVersion(JNIEnv* env) {
    CHECK_JNI_ENTRY(kFlag_Default, "E", env);
    return CHECK_JNI_EXIT("I", baseEnv(env)->GetVersion(env));
}

static jclass Check_DefineClass(JNIEnv* env, const char* name, jobject loader,
    const jbyte* buf, jsize bufLen)
{
    CHECK_JNI_ENTRY(kFlag_Default, "EuLpz", env, name, loader, buf, bufLen);
    sc.checkClassName(name);
    return CHECK_JNI_EXIT("c", baseEnv(env)->DefineClass(env, name, loader, buf, bufLen));
}

static jclass Check_FindClass(JNIEnv* env, const char* name) {
    CHECK_JNI_ENTRY(kFlag_Default, "Eu", env, name);
    sc.checkClassName(name);
    return CHECK_JNI_EXIT("c", baseEnv(env)->FindClass(env, name));
}

static jclass Check_GetSuperclass(JNIEnv* env, jclass clazz) {
    CHECK_JNI_ENTRY(kFlag_Default, "Ec", env, clazz);
    return CHECK_JNI_EXIT("c", baseEnv(env)->GetSuperclass(env, clazz));
}

static jboolean Check_IsAssignableFrom(JNIEnv* env, jclass clazz1, jclass clazz2) {
    CHECK_JNI_ENTRY(kFlag_Default, "Ecc", env, clazz1, clazz2);
    return CHECK_JNI_EXIT("b", baseEnv(env)->IsAssignableFrom(env, clazz1, clazz2));
}

static jmethodID Check_FromReflectedMethod(JNIEnv* env, jobject method) {
    CHECK_JNI_ENTRY(kFlag_Default, "EL", env, method);
    // TODO: check that 'field' is a java.lang.reflect.Method.
    return CHECK_JNI_EXIT("m", baseEnv(env)->FromReflectedMethod(env, method));
}

static jfieldID Check_FromReflectedField(JNIEnv* env, jobject field) {
    CHECK_JNI_ENTRY(kFlag_Default, "EL", env, field);
    // TODO: check that 'field' is a java.lang.reflect.Field.
    return CHECK_JNI_EXIT("f", baseEnv(env)->FromReflectedField(env, field));
}

static jobject Check_ToReflectedMethod(JNIEnv* env, jclass cls,
        jmethodID methodID, jboolean isStatic)
{
    CHECK_JNI_ENTRY(kFlag_Default, "Ecmb", env, cls, methodID, isStatic);
    return CHECK_JNI_EXIT("L", baseEnv(env)->ToReflectedMethod(env, cls, methodID, isStatic));
}

static jobject Check_ToReflectedField(JNIEnv* env, jclass cls,
        jfieldID fieldID, jboolean isStatic)
{
    CHECK_JNI_ENTRY(kFlag_Default, "Ecfb", env, cls, fieldID, isStatic);
    return CHECK_JNI_EXIT("L", baseEnv(env)->ToReflectedField(env, cls, fieldID, isStatic));
}

static jint Check_Throw(JNIEnv* env, jthrowable obj) {
    CHECK_JNI_ENTRY(kFlag_Default, "EL", env, obj);
    // TODO: check that 'obj' is a java.lang.Throwable.
    return CHECK_JNI_EXIT("I", baseEnv(env)->Throw(env, obj));
}

static jint Check_ThrowNew(JNIEnv* env, jclass clazz, const char* message) {
    CHECK_JNI_ENTRY(kFlag_NullableUtf, "Ecu", env, clazz, message);
    return CHECK_JNI_EXIT("I", baseEnv(env)->ThrowNew(env, clazz, message));
}

static jthrowable Check_ExceptionOccurred(JNIEnv* env) {
    CHECK_JNI_ENTRY(kFlag_ExcepOkay, "E", env);
    return CHECK_JNI_EXIT("L", baseEnv(env)->ExceptionOccurred(env));
}

static void Check_ExceptionDescribe(JNIEnv* env) {
    CHECK_JNI_ENTRY(kFlag_ExcepOkay, "E", env);
    baseEnv(env)->ExceptionDescribe(env);
    CHECK_JNI_EXIT_VOID();
}

static void Check_ExceptionClear(JNIEnv* env) {
    CHECK_JNI_ENTRY(kFlag_ExcepOkay, "E", env);
    baseEnv(env)->ExceptionClear(env);
    CHECK_JNI_EXIT_VOID();
}

static void Check_FatalError(JNIEnv* env, const char* msg) {
    CHECK_JNI_ENTRY(kFlag_NullableUtf, "Eu", env, msg);
    baseEnv(env)->FatalError(env, msg);
    CHECK_JNI_EXIT_VOID();
}

static jint Check_PushLocalFrame(JNIEnv* env, jint capacity) {
    CHECK_JNI_ENTRY(kFlag_Default | kFlag_ExcepOkay, "EI", env, capacity);
    return CHECK_JNI_EXIT("I", baseEnv(env)->PushLocalFrame(env, capacity));
}

static jobject Check_PopLocalFrame(JNIEnv* env, jobject res) {
    CHECK_JNI_ENTRY(kFlag_Default | kFlag_ExcepOkay, "EL", env, res);
    return CHECK_JNI_EXIT("L", baseEnv(env)->PopLocalFrame(env, res));
}

static jobject Check_NewGlobalRef(JNIEnv* env, jobject obj) {
    CHECK_JNI_ENTRY(kFlag_Default, "EL", env, obj);
    return CHECK_JNI_EXIT("L", baseEnv(env)->NewGlobalRef(env, obj));
}

static void Check_DeleteGlobalRef(JNIEnv* env, jobject globalRef) {
    CHECK_JNI_ENTRY(kFlag_Default | kFlag_ExcepOkay, "EL", env, globalRef);
    if (globalRef != NULL && dvmGetJNIRefType(sc.self(), globalRef) != JNIGlobalRefType) {
        ALOGW("JNI WARNING: DeleteGlobalRef on non-global %p (type=%d)",
             globalRef, dvmGetJNIRefType(sc.self(), globalRef));
        abortMaybe();
    } else {
        baseEnv(env)->DeleteGlobalRef(env, globalRef);
        CHECK_JNI_EXIT_VOID();
    }
}

static jobject Check_NewLocalRef(JNIEnv* env, jobject ref) {
    CHECK_JNI_ENTRY(kFlag_Default, "EL", env, ref);
    return CHECK_JNI_EXIT("L", baseEnv(env)->NewLocalRef(env, ref));
}

static void Check_DeleteLocalRef(JNIEnv* env, jobject localRef) {
    CHECK_JNI_ENTRY(kFlag_Default | kFlag_ExcepOkay, "EL", env, localRef);
    if (localRef != NULL && dvmGetJNIRefType(sc.self(), localRef) != JNILocalRefType) {
        ALOGW("JNI WARNING: DeleteLocalRef on non-local %p (type=%d)",
             localRef, dvmGetJNIRefType(sc.self(), localRef));
        abortMaybe();
    } else {
        baseEnv(env)->DeleteLocalRef(env, localRef);
        CHECK_JNI_EXIT_VOID();
    }
}

static jint Check_EnsureLocalCapacity(JNIEnv *env, jint capacity) {
    CHECK_JNI_ENTRY(kFlag_Default, "EI", env, capacity);
    return CHECK_JNI_EXIT("I", baseEnv(env)->EnsureLocalCapacity(env, capacity));
}

static jboolean Check_IsSameObject(JNIEnv* env, jobject ref1, jobject ref2) {
    CHECK_JNI_ENTRY(kFlag_Default, "ELL", env, ref1, ref2);
    return CHECK_JNI_EXIT("b", baseEnv(env)->IsSameObject(env, ref1, ref2));
}

static jobject Check_AllocObject(JNIEnv* env, jclass clazz) {
    CHECK_JNI_ENTRY(kFlag_Default, "Ec", env, clazz);
    return CHECK_JNI_EXIT("L", baseEnv(env)->AllocObject(env, clazz));
}

static jobject Check_NewObject(JNIEnv* env, jclass clazz, jmethodID methodID, ...) {
    CHECK_JNI_ENTRY(kFlag_Default, "Ecm.", env, clazz, methodID);
    va_list args;
    va_start(args, methodID);
    jobject result = baseEnv(env)->NewObjectV(env, clazz, methodID, args);
    va_end(args);
    return CHECK_JNI_EXIT("L", result);
}

static jobject Check_NewObjectV(JNIEnv* env, jclass clazz, jmethodID methodID, va_list args) {
    CHECK_JNI_ENTRY(kFlag_Default, "Ecm.", env, clazz, methodID);
    return CHECK_JNI_EXIT("L", baseEnv(env)->NewObjectV(env, clazz, methodID, args));
}

static jobject Check_NewObjectA(JNIEnv* env, jclass clazz, jmethodID methodID, jvalue* args) {
    CHECK_JNI_ENTRY(kFlag_Default, "Ecm.", env, clazz, methodID);
    return CHECK_JNI_EXIT("L", baseEnv(env)->NewObjectA(env, clazz, methodID, args));
}

static jclass Check_GetObjectClass(JNIEnv* env, jobject obj) {
    CHECK_JNI_ENTRY(kFlag_Default, "EL", env, obj);
    return CHECK_JNI_EXIT("c", baseEnv(env)->GetObjectClass(env, obj));
}

static jboolean Check_IsInstanceOf(JNIEnv* env, jobject obj, jclass clazz) {
    CHECK_JNI_ENTRY(kFlag_Default, "ELc", env, obj, clazz);
    return CHECK_JNI_EXIT("b", baseEnv(env)->IsInstanceOf(env, obj, clazz));
}

static jmethodID Check_GetMethodID(JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    CHECK_JNI_ENTRY(kFlag_Default, "Ecuu", env, clazz, name, sig);
    return CHECK_JNI_EXIT("m", baseEnv(env)->GetMethodID(env, clazz, name, sig));
}

static jfieldID Check_GetFieldID(JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    CHECK_JNI_ENTRY(kFlag_Default, "Ecuu", env, clazz, name, sig);
    return CHECK_JNI_EXIT("f", baseEnv(env)->GetFieldID(env, clazz, name, sig));
}

static jmethodID Check_GetStaticMethodID(JNIEnv* env, jclass clazz,
        const char* name, const char* sig)
{
    CHECK_JNI_ENTRY(kFlag_Default, "Ecuu", env, clazz, name, sig);
    return CHECK_JNI_EXIT("m", baseEnv(env)->GetStaticMethodID(env, clazz, name, sig));
}

static jfieldID Check_GetStaticFieldID(JNIEnv* env, jclass clazz,
        const char* name, const char* sig)
{
    CHECK_JNI_ENTRY(kFlag_Default, "Ecuu", env, clazz, name, sig);
    return CHECK_JNI_EXIT("f", baseEnv(env)->GetStaticFieldID(env, clazz, name, sig));
}

#define FIELD_ACCESSORS(_ctype, _jname, _ftype, _type) \
    static _ctype Check_GetStatic##_jname##Field(JNIEnv* env, jclass clazz, jfieldID fieldID) { \
        CHECK_JNI_ENTRY(kFlag_Default, "Ecf", env, clazz, fieldID); \
        sc.checkStaticFieldID(clazz, fieldID); \
        sc.checkFieldTypeForGet(fieldID, _type, true); \
        return CHECK_JNI_EXIT(_type, baseEnv(env)->GetStatic##_jname##Field(env, clazz, fieldID)); \
    } \
    static _ctype Check_Get##_jname##Field(JNIEnv* env, jobject obj, jfieldID fieldID) { \
        CHECK_JNI_ENTRY(kFlag_Default, "ELf", env, obj, fieldID); \
        sc.checkInstanceFieldID(obj, fieldID); \
        sc.checkFieldTypeForGet(fieldID, _type, false); \
        return CHECK_JNI_EXIT(_type, baseEnv(env)->Get##_jname##Field(env, obj, fieldID)); \
    } \
    static void Check_SetStatic##_jname##Field(JNIEnv* env, jclass clazz, jfieldID fieldID, _ctype value) { \
        CHECK_JNI_ENTRY(kFlag_Default, "Ecf" _type, env, clazz, fieldID, value); \
        sc.checkStaticFieldID(clazz, fieldID); \
        /* "value" arg only used when type == ref */ \
        sc.checkFieldTypeForSet((jobject)(u4)value, fieldID, _ftype, true); \
        baseEnv(env)->SetStatic##_jname##Field(env, clazz, fieldID, value); \
        CHECK_JNI_EXIT_VOID(); \
    } \
    static void Check_Set##_jname##Field(JNIEnv* env, jobject obj, jfieldID fieldID, _ctype value) { \
        CHECK_JNI_ENTRY(kFlag_Default, "ELf" _type, env, obj, fieldID, value); \
        sc.checkInstanceFieldID(obj, fieldID); \
        /* "value" arg only used when type == ref */ \
        sc.checkFieldTypeForSet((jobject)(u4) value, fieldID, _ftype, false); \
        baseEnv(env)->Set##_jname##Field(env, obj, fieldID, value); \
        CHECK_JNI_EXIT_VOID(); \
    }

FIELD_ACCESSORS(jobject, Object, PRIM_NOT, "L");
FIELD_ACCESSORS(jboolean, Boolean, PRIM_BOOLEAN, "Z");
FIELD_ACCESSORS(jbyte, Byte, PRIM_BYTE, "B");
FIELD_ACCESSORS(jchar, Char, PRIM_CHAR, "C");
FIELD_ACCESSORS(jshort, Short, PRIM_SHORT, "S");
FIELD_ACCESSORS(jint, Int, PRIM_INT, "I");
FIELD_ACCESSORS(jlong, Long, PRIM_LONG, "J");
FIELD_ACCESSORS(jfloat, Float, PRIM_FLOAT, "F");
FIELD_ACCESSORS(jdouble, Double, PRIM_DOUBLE, "D");

#define CALL(_ctype, _jname, _retdecl, _retasgn, _retok, _retsig) \
    /* Virtual... */ \
    static _ctype Check_Call##_jname##Method(JNIEnv* env, jobject obj, \
        jmethodID methodID, ...) \
    { \
        CHECK_JNI_ENTRY(kFlag_Default, "ELm.", env, obj, methodID); /* TODO: args! */ \
        sc.checkSig(methodID, _retsig, false); \
        sc.checkVirtualMethod(obj, methodID); \
        _retdecl; \
        va_list args; \
        va_start(args, methodID); \
        _retasgn baseEnv(env)->Call##_jname##MethodV(env, obj, methodID, args); \
        va_end(args); \
        _retok; \
    } \
    static _ctype Check_Call##_jname##MethodV(JNIEnv* env, jobject obj, \
        jmethodID methodID, va_list args) \
    { \
        CHECK_JNI_ENTRY(kFlag_Default, "ELm.", env, obj, methodID); /* TODO: args! */ \
        sc.checkSig(methodID, _retsig, false); \
        sc.checkVirtualMethod(obj, methodID); \
        _retdecl; \
        _retasgn baseEnv(env)->Call##_jname##MethodV(env, obj, methodID, args); \
        _retok; \
    } \
    static _ctype Check_Call##_jname##MethodA(JNIEnv* env, jobject obj, \
        jmethodID methodID, jvalue* args) \
    { \
        CHECK_JNI_ENTRY(kFlag_Default, "ELm.", env, obj, methodID); /* TODO: args! */ \
        sc.checkSig(methodID, _retsig, false); \
        sc.checkVirtualMethod(obj, methodID); \
        _retdecl; \
        _retasgn baseEnv(env)->Call##_jname##MethodA(env, obj, methodID, args); \
        _retok; \
    } \
    /* Non-virtual... */ \
    static _ctype Check_CallNonvirtual##_jname##Method(JNIEnv* env, \
        jobject obj, jclass clazz, jmethodID methodID, ...) \
    { \
        CHECK_JNI_ENTRY(kFlag_Default, "ELcm.", env, obj, clazz, methodID); /* TODO: args! */ \
        sc.checkSig(methodID, _retsig, false); \
        sc.checkVirtualMethod(obj, methodID); \
        _retdecl; \
        va_list args; \
        va_start(args, methodID); \
        _retasgn baseEnv(env)->CallNonvirtual##_jname##MethodV(env, obj, clazz, methodID, args); \
        va_end(args); \
        _retok; \
    } \
    static _ctype Check_CallNonvirtual##_jname##MethodV(JNIEnv* env, \
        jobject obj, jclass clazz, jmethodID methodID, va_list args) \
    { \
        CHECK_JNI_ENTRY(kFlag_Default, "ELcm.", env, obj, clazz, methodID); /* TODO: args! */ \
        sc.checkSig(methodID, _retsig, false); \
        sc.checkVirtualMethod(obj, methodID); \
        _retdecl; \
        _retasgn baseEnv(env)->CallNonvirtual##_jname##MethodV(env, obj, clazz, methodID, args); \
        _retok; \
    } \
    static _ctype Check_CallNonvirtual##_jname##MethodA(JNIEnv* env, \
        jobject obj, jclass clazz, jmethodID methodID, jvalue* args) \
    { \
        CHECK_JNI_ENTRY(kFlag_Default, "ELcm.", env, obj, clazz, methodID); /* TODO: args! */ \
        sc.checkSig(methodID, _retsig, false); \
        sc.checkVirtualMethod(obj, methodID); \
        _retdecl; \
        _retasgn baseEnv(env)->CallNonvirtual##_jname##MethodA(env, obj, clazz, methodID, args); \
        _retok; \
    } \
    /* Static... */ \
    static _ctype Check_CallStatic##_jname##Method(JNIEnv* env, \
        jclass clazz, jmethodID methodID, ...) \
    { \
        CHECK_JNI_ENTRY(kFlag_Default, "Ecm.", env, clazz, methodID); /* TODO: args! */ \
        sc.checkSig(methodID, _retsig, true); \
        sc.checkStaticMethod(clazz, methodID); \
        _retdecl; \
        va_list args; \
        va_start(args, methodID); \
        _retasgn baseEnv(env)->CallStatic##_jname##MethodV(env, clazz, methodID, args); \
        va_end(args); \
        _retok; \
    } \
    static _ctype Check_CallStatic##_jname##MethodV(JNIEnv* env, \
        jclass clazz, jmethodID methodID, va_list args) \
    { \
        CHECK_JNI_ENTRY(kFlag_Default, "Ecm.", env, clazz, methodID); /* TODO: args! */ \
        sc.checkSig(methodID, _retsig, true); \
        sc.checkStaticMethod(clazz, methodID); \
        _retdecl; \
        _retasgn baseEnv(env)->CallStatic##_jname##MethodV(env, clazz, methodID, args); \
        _retok; \
    } \
    static _ctype Check_CallStatic##_jname##MethodA(JNIEnv* env, \
        jclass clazz, jmethodID methodID, jvalue* args) \
    { \
        CHECK_JNI_ENTRY(kFlag_Default, "Ecm.", env, clazz, methodID); /* TODO: args! */ \
        sc.checkSig(methodID, _retsig, true); \
        sc.checkStaticMethod(clazz, methodID); \
        _retdecl; \
        _retasgn baseEnv(env)->CallStatic##_jname##MethodA(env, clazz, methodID, args); \
        _retok; \
    }

#define NON_VOID_RETURN(_retsig, _ctype) return CHECK_JNI_EXIT(_retsig, (_ctype) result)
#define VOID_RETURN CHECK_JNI_EXIT_VOID()

CALL(jobject, Object, Object* result, result=(Object*), NON_VOID_RETURN("L", jobject), "L");
CALL(jboolean, Boolean, jboolean result, result=, NON_VOID_RETURN("Z", jboolean), "Z");
CALL(jbyte, Byte, jbyte result, result=, NON_VOID_RETURN("B", jbyte), "B");
CALL(jchar, Char, jchar result, result=, NON_VOID_RETURN("C", jchar), "C");
CALL(jshort, Short, jshort result, result=, NON_VOID_RETURN("S", jshort), "S");
CALL(jint, Int, jint result, result=, NON_VOID_RETURN("I", jint), "I");
CALL(jlong, Long, jlong result, result=, NON_VOID_RETURN("J", jlong), "J");
CALL(jfloat, Float, jfloat result, result=, NON_VOID_RETURN("F", jfloat), "F");
CALL(jdouble, Double, jdouble result, result=, NON_VOID_RETURN("D", jdouble), "D");
CALL(void, Void, , , VOID_RETURN, "V");

static jstring Check_NewString(JNIEnv* env, const jchar* unicodeChars, jsize len) {
    CHECK_JNI_ENTRY(kFlag_Default, "Epz", env, unicodeChars, len);
    return CHECK_JNI_EXIT("s", baseEnv(env)->NewString(env, unicodeChars, len));
}

static jsize Check_GetStringLength(JNIEnv* env, jstring string) {
    CHECK_JNI_ENTRY(kFlag_CritOkay, "Es", env, string);
    return CHECK_JNI_EXIT("I", baseEnv(env)->GetStringLength(env, string));
}

static const jchar* Check_GetStringChars(JNIEnv* env, jstring string, jboolean* isCopy) {
    CHECK_JNI_ENTRY(kFlag_CritOkay, "Esp", env, string, isCopy);
    const jchar* result = baseEnv(env)->GetStringChars(env, string, isCopy);
    if (gDvmJni.forceCopy && result != NULL) {
        ScopedCheckJniThreadState ts(env);
        StringObject* strObj = (StringObject*) dvmDecodeIndirectRef(dvmThreadSelf(), string);
        int byteCount = strObj->length() * 2;
        result = (const jchar*) GuardedCopy::create(result, byteCount, false);
        if (isCopy != NULL) {
            *isCopy = JNI_TRUE;
        }
    }
    return CHECK_JNI_EXIT("p", result);
}

static void Check_ReleaseStringChars(JNIEnv* env, jstring string, const jchar* chars) {
    CHECK_JNI_ENTRY(kFlag_Default | kFlag_ExcepOkay, "Esp", env, string, chars);
    sc.checkNonNull(chars);
    if (gDvmJni.forceCopy) {
        if (!GuardedCopy::check(chars, false)) {
            ALOGE("JNI: failed guarded copy check in ReleaseStringChars");
            abortMaybe();
            return;
        }
        chars = (const jchar*) GuardedCopy::destroy((jchar*)chars);
    }
    baseEnv(env)->ReleaseStringChars(env, string, chars);
    CHECK_JNI_EXIT_VOID();
}

static jstring Check_NewStringUTF(JNIEnv* env, const char* bytes) {
    CHECK_JNI_ENTRY(kFlag_NullableUtf, "Eu", env, bytes); // TODO: show pointer and truncate string.
    return CHECK_JNI_EXIT("s", baseEnv(env)->NewStringUTF(env, bytes));
}

static jsize Check_GetStringUTFLength(JNIEnv* env, jstring string) {
    CHECK_JNI_ENTRY(kFlag_CritOkay, "Es", env, string);
    return CHECK_JNI_EXIT("I", baseEnv(env)->GetStringUTFLength(env, string));
}

static const char* Check_GetStringUTFChars(JNIEnv* env, jstring string, jboolean* isCopy) {
    CHECK_JNI_ENTRY(kFlag_CritOkay, "Esp", env, string, isCopy);
    const char* result = baseEnv(env)->GetStringUTFChars(env, string, isCopy);
    if (gDvmJni.forceCopy && result != NULL) {
        result = (const char*) GuardedCopy::create(result, strlen(result) + 1, false);
        if (isCopy != NULL) {
            *isCopy = JNI_TRUE;
        }
    }
    return CHECK_JNI_EXIT("u", result); // TODO: show pointer and truncate string.
}

static void Check_ReleaseStringUTFChars(JNIEnv* env, jstring string, const char* utf) {
    CHECK_JNI_ENTRY(kFlag_ExcepOkay | kFlag_Release, "Esu", env, string, utf); // TODO: show pointer and truncate string.
    if (gDvmJni.forceCopy) {
        if (!GuardedCopy::check(utf, false)) {
            ALOGE("JNI: failed guarded copy check in ReleaseStringUTFChars");
            abortMaybe();
            return;
        }
        utf = (const char*) GuardedCopy::destroy((char*)utf);
    }
    baseEnv(env)->ReleaseStringUTFChars(env, string, utf);
    CHECK_JNI_EXIT_VOID();
}

static jsize Check_GetArrayLength(JNIEnv* env, jarray array) {
    CHECK_JNI_ENTRY(kFlag_CritOkay, "Ea", env, array);
    return CHECK_JNI_EXIT("I", baseEnv(env)->GetArrayLength(env, array));
}

static jobjectArray Check_NewObjectArray(JNIEnv* env, jsize length,
        jclass elementClass, jobject initialElement)
{
    CHECK_JNI_ENTRY(kFlag_Default, "EzcL", env, length, elementClass, initialElement);
    return CHECK_JNI_EXIT("a", baseEnv(env)->NewObjectArray(env, length, elementClass, initialElement));
}

static jobject Check_GetObjectArrayElement(JNIEnv* env, jobjectArray array, jsize index) {
    CHECK_JNI_ENTRY(kFlag_Default, "EaI", env, array, index);
    return CHECK_JNI_EXIT("L", baseEnv(env)->GetObjectArrayElement(env, array, index));
}

static void Check_SetObjectArrayElement(JNIEnv* env, jobjectArray array, jsize index, jobject value)
{
    CHECK_JNI_ENTRY(kFlag_Default, "EaIL", env, array, index, value);
    baseEnv(env)->SetObjectArrayElement(env, array, index, value);
    CHECK_JNI_EXIT_VOID();
}

#define NEW_PRIMITIVE_ARRAY(_artype, _jname) \
    static _artype Check_New##_jname##Array(JNIEnv* env, jsize length) { \
        CHECK_JNI_ENTRY(kFlag_Default, "Ez", env, length); \
        return CHECK_JNI_EXIT("a", baseEnv(env)->New##_jname##Array(env, length)); \
    }
NEW_PRIMITIVE_ARRAY(jbooleanArray, Boolean);
NEW_PRIMITIVE_ARRAY(jbyteArray, Byte);
NEW_PRIMITIVE_ARRAY(jcharArray, Char);
NEW_PRIMITIVE_ARRAY(jshortArray, Short);
NEW_PRIMITIVE_ARRAY(jintArray, Int);
NEW_PRIMITIVE_ARRAY(jlongArray, Long);
NEW_PRIMITIVE_ARRAY(jfloatArray, Float);
NEW_PRIMITIVE_ARRAY(jdoubleArray, Double);


/*
 * Hack to allow forcecopy to work with jniGetNonMovableArrayElements.
 * The code deliberately uses an invalid sequence of operations, so we
 * need to pass it through unmodified.  Review that code before making
 * any changes here.
 */
#define kNoCopyMagic    0xd5aab57f

#define GET_PRIMITIVE_ARRAY_ELEMENTS(_ctype, _jname) \
    static _ctype* Check_Get##_jname##ArrayElements(JNIEnv* env, \
        _ctype##Array array, jboolean* isCopy) \
    { \
        CHECK_JNI_ENTRY(kFlag_Default, "Eap", env, array, isCopy); \
        u4 noCopy = 0; \
        if (gDvmJni.forceCopy && isCopy != NULL) { \
            /* capture this before the base call tramples on it */ \
            noCopy = *(u4*) isCopy; \
        } \
        _ctype* result = baseEnv(env)->Get##_jname##ArrayElements(env, array, isCopy); \
        if (gDvmJni.forceCopy && result != NULL) { \
            if (noCopy == kNoCopyMagic) { \
                ALOGV("FC: not copying %p %x", array, noCopy); \
            } else { \
                result = (_ctype*) createGuardedPACopy(env, array, isCopy); \
            } \
        } \
        return CHECK_JNI_EXIT("p", result); \
    }

#define RELEASE_PRIMITIVE_ARRAY_ELEMENTS(_ctype, _jname) \
    static void Check_Release##_jname##ArrayElements(JNIEnv* env, \
        _ctype##Array array, _ctype* elems, jint mode) \
    { \
        CHECK_JNI_ENTRY(kFlag_Default | kFlag_ExcepOkay, "Eapr", env, array, elems, mode); \
        sc.checkNonNull(elems); \
        if (gDvmJni.forceCopy) { \
            if ((uintptr_t)elems == kNoCopyMagic) { \
                ALOGV("FC: not freeing %p", array); \
                elems = NULL;   /* base JNI call doesn't currently need */ \
            } else { \
                elems = (_ctype*) releaseGuardedPACopy(env, array, elems, mode); \
            } \
        } \
        baseEnv(env)->Release##_jname##ArrayElements(env, array, elems, mode); \
        CHECK_JNI_EXIT_VOID(); \
    }

#define GET_PRIMITIVE_ARRAY_REGION(_ctype, _jname) \
    static void Check_Get##_jname##ArrayRegion(JNIEnv* env, \
            _ctype##Array array, jsize start, jsize len, _ctype* buf) { \
        CHECK_JNI_ENTRY(kFlag_Default, "EaIIp", env, array, start, len, buf); \
        baseEnv(env)->Get##_jname##ArrayRegion(env, array, start, len, buf); \
        CHECK_JNI_EXIT_VOID(); \
    }

#define SET_PRIMITIVE_ARRAY_REGION(_ctype, _jname) \
    static void Check_Set##_jname##ArrayRegion(JNIEnv* env, \
            _ctype##Array array, jsize start, jsize len, const _ctype* buf) { \
        CHECK_JNI_ENTRY(kFlag_Default, "EaIIp", env, array, start, len, buf); \
        baseEnv(env)->Set##_jname##ArrayRegion(env, array, start, len, buf); \
        CHECK_JNI_EXIT_VOID(); \
    }

#define PRIMITIVE_ARRAY_FUNCTIONS(_ctype, _jname, _typechar) \
    GET_PRIMITIVE_ARRAY_ELEMENTS(_ctype, _jname); \
    RELEASE_PRIMITIVE_ARRAY_ELEMENTS(_ctype, _jname); \
    GET_PRIMITIVE_ARRAY_REGION(_ctype, _jname); \
    SET_PRIMITIVE_ARRAY_REGION(_ctype, _jname);

/* TODO: verify primitive array type matches call type */
PRIMITIVE_ARRAY_FUNCTIONS(jboolean, Boolean, 'Z');
PRIMITIVE_ARRAY_FUNCTIONS(jbyte, Byte, 'B');
PRIMITIVE_ARRAY_FUNCTIONS(jchar, Char, 'C');
PRIMITIVE_ARRAY_FUNCTIONS(jshort, Short, 'S');
PRIMITIVE_ARRAY_FUNCTIONS(jint, Int, 'I');
PRIMITIVE_ARRAY_FUNCTIONS(jlong, Long, 'J');
PRIMITIVE_ARRAY_FUNCTIONS(jfloat, Float, 'F');
PRIMITIVE_ARRAY_FUNCTIONS(jdouble, Double, 'D');

static jint Check_RegisterNatives(JNIEnv* env, jclass clazz, const JNINativeMethod* methods,
        jint nMethods)
{
    CHECK_JNI_ENTRY(kFlag_Default, "EcpI", env, clazz, methods, nMethods);
    return CHECK_JNI_EXIT("I", baseEnv(env)->RegisterNatives(env, clazz, methods, nMethods));
}

static jint Check_UnregisterNatives(JNIEnv* env, jclass clazz) {
    CHECK_JNI_ENTRY(kFlag_Default, "Ec", env, clazz);
    return CHECK_JNI_EXIT("I", baseEnv(env)->UnregisterNatives(env, clazz));
}

static jint Check_MonitorEnter(JNIEnv* env, jobject obj) {
    CHECK_JNI_ENTRY(kFlag_Default, "EL", env, obj);
    return CHECK_JNI_EXIT("I", baseEnv(env)->MonitorEnter(env, obj));
}

static jint Check_MonitorExit(JNIEnv* env, jobject obj) {
    CHECK_JNI_ENTRY(kFlag_Default | kFlag_ExcepOkay, "EL", env, obj);
    return CHECK_JNI_EXIT("I", baseEnv(env)->MonitorExit(env, obj));
}

static jint Check_GetJavaVM(JNIEnv *env, JavaVM **vm) {
    CHECK_JNI_ENTRY(kFlag_Default, "Ep", env, vm);
    return CHECK_JNI_EXIT("I", baseEnv(env)->GetJavaVM(env, vm));
}

static void Check_GetStringRegion(JNIEnv* env, jstring str, jsize start, jsize len, jchar* buf) {
    CHECK_JNI_ENTRY(kFlag_CritOkay, "EsIIp", env, str, start, len, buf);
    baseEnv(env)->GetStringRegion(env, str, start, len, buf);
    CHECK_JNI_EXIT_VOID();
}

static void Check_GetStringUTFRegion(JNIEnv* env, jstring str, jsize start, jsize len, char* buf) {
    CHECK_JNI_ENTRY(kFlag_CritOkay, "EsIIp", env, str, start, len, buf);
    baseEnv(env)->GetStringUTFRegion(env, str, start, len, buf);
    CHECK_JNI_EXIT_VOID();
}

static void* Check_GetPrimitiveArrayCritical(JNIEnv* env, jarray array, jboolean* isCopy) {
    CHECK_JNI_ENTRY(kFlag_CritGet, "Eap", env, array, isCopy);
    void* result = baseEnv(env)->GetPrimitiveArrayCritical(env, array, isCopy);
    if (gDvmJni.forceCopy && result != NULL) {
        result = createGuardedPACopy(env, array, isCopy);
    }
    return CHECK_JNI_EXIT("p", result);
}

static void Check_ReleasePrimitiveArrayCritical(JNIEnv* env, jarray array, void* carray, jint mode)
{
    CHECK_JNI_ENTRY(kFlag_CritRelease | kFlag_ExcepOkay, "Eapr", env, array, carray, mode);
    sc.checkNonNull(carray);
    if (gDvmJni.forceCopy) {
        carray = releaseGuardedPACopy(env, array, carray, mode);
    }
    baseEnv(env)->ReleasePrimitiveArrayCritical(env, array, carray, mode);
    CHECK_JNI_EXIT_VOID();
}

static const jchar* Check_GetStringCritical(JNIEnv* env, jstring string, jboolean* isCopy) {
    CHECK_JNI_ENTRY(kFlag_CritGet, "Esp", env, string, isCopy);
    const jchar* result = baseEnv(env)->GetStringCritical(env, string, isCopy);
    if (gDvmJni.forceCopy && result != NULL) {
        ScopedCheckJniThreadState ts(env);
        StringObject* strObj = (StringObject*) dvmDecodeIndirectRef(dvmThreadSelf(), string);
        int byteCount = strObj->length() * 2;
        result = (const jchar*) GuardedCopy::create(result, byteCount, false);
        if (isCopy != NULL) {
            *isCopy = JNI_TRUE;
        }
    }
    return CHECK_JNI_EXIT("p", result);
}

static void Check_ReleaseStringCritical(JNIEnv* env, jstring string, const jchar* carray) {
    CHECK_JNI_ENTRY(kFlag_CritRelease | kFlag_ExcepOkay, "Esp", env, string, carray);
    sc.checkNonNull(carray);
    if (gDvmJni.forceCopy) {
        if (!GuardedCopy::check(carray, false)) {
            ALOGE("JNI: failed guarded copy check in ReleaseStringCritical");
            abortMaybe();
            return;
        }
        carray = (const jchar*) GuardedCopy::destroy((jchar*)carray);
    }
    baseEnv(env)->ReleaseStringCritical(env, string, carray);
    CHECK_JNI_EXIT_VOID();
}

static jweak Check_NewWeakGlobalRef(JNIEnv* env, jobject obj) {
    CHECK_JNI_ENTRY(kFlag_Default, "EL", env, obj);
    return CHECK_JNI_EXIT("L", baseEnv(env)->NewWeakGlobalRef(env, obj));
}

static void Check_DeleteWeakGlobalRef(JNIEnv* env, jweak obj) {
    CHECK_JNI_ENTRY(kFlag_Default | kFlag_ExcepOkay, "EL", env, obj);
    baseEnv(env)->DeleteWeakGlobalRef(env, obj);
    CHECK_JNI_EXIT_VOID();
}

static jboolean Check_ExceptionCheck(JNIEnv* env) {
    CHECK_JNI_ENTRY(kFlag_CritOkay | kFlag_ExcepOkay, "E", env);
    return CHECK_JNI_EXIT("b", baseEnv(env)->ExceptionCheck(env));
}

static jobjectRefType Check_GetObjectRefType(JNIEnv* env, jobject obj) {
    CHECK_JNI_ENTRY(kFlag_Default, "EL", env, obj);
    // TODO: proper decoding of jobjectRefType!
    return CHECK_JNI_EXIT("I", baseEnv(env)->GetObjectRefType(env, obj));
}

static jobject Check_NewDirectByteBuffer(JNIEnv* env, void* address, jlong capacity) {
    CHECK_JNI_ENTRY(kFlag_Default, "EpJ", env, address, capacity);
    return CHECK_JNI_EXIT("L", baseEnv(env)->NewDirectByteBuffer(env, address, capacity));
}

static void* Check_GetDirectBufferAddress(JNIEnv* env, jobject buf) {
    CHECK_JNI_ENTRY(kFlag_Default, "EL", env, buf);
    // TODO: check that 'buf' is a java.nio.Buffer.
    return CHECK_JNI_EXIT("p", baseEnv(env)->GetDirectBufferAddress(env, buf));
}

static jlong Check_GetDirectBufferCapacity(JNIEnv* env, jobject buf) {
    CHECK_JNI_ENTRY(kFlag_Default, "EL", env, buf);
    // TODO: check that 'buf' is a java.nio.Buffer.
    return CHECK_JNI_EXIT("J", baseEnv(env)->GetDirectBufferCapacity(env, buf));
}


/*
 * ===========================================================================
 *      JNI invocation functions
 * ===========================================================================
 */

static jint Check_DestroyJavaVM(JavaVM* vm) {
    ScopedCheck sc(false, __FUNCTION__);
    sc.check(true, "v", vm);
    return CHECK_JNI_EXIT("I", baseVm(vm)->DestroyJavaVM(vm));
}

static jint Check_AttachCurrentThread(JavaVM* vm, JNIEnv** p_env, void* thr_args) {
    ScopedCheck sc(false, __FUNCTION__);
    sc.check(true, "vpt", vm, p_env, thr_args);
    return CHECK_JNI_EXIT("I", baseVm(vm)->AttachCurrentThread(vm, p_env, thr_args));
}

static jint Check_AttachCurrentThreadAsDaemon(JavaVM* vm, JNIEnv** p_env, void* thr_args) {
    ScopedCheck sc(false, __FUNCTION__);
    sc.check(true, "vpt", vm, p_env, thr_args);
    return CHECK_JNI_EXIT("I", baseVm(vm)->AttachCurrentThreadAsDaemon(vm, p_env, thr_args));
}

static jint Check_DetachCurrentThread(JavaVM* vm) {
    ScopedCheck sc(true, __FUNCTION__);
    sc.check(true, "v", vm);
    return CHECK_JNI_EXIT("I", baseVm(vm)->DetachCurrentThread(vm));
}

static jint Check_GetEnv(JavaVM* vm, void** env, jint version) {
    ScopedCheck sc(true, __FUNCTION__);
    sc.check(true, "v", vm);
    return CHECK_JNI_EXIT("I", baseVm(vm)->GetEnv(vm, env, version));
}


/*
 * ===========================================================================
 *      Function tables
 * ===========================================================================
 */

static const struct JNINativeInterface gCheckNativeInterface = {
    NULL,
    NULL,
    NULL,
    NULL,

    Check_GetVersion,

    Check_DefineClass,
    Check_FindClass,

    Check_FromReflectedMethod,
    Check_FromReflectedField,
    Check_ToReflectedMethod,

    Check_GetSuperclass,
    Check_IsAssignableFrom,

    Check_ToReflectedField,

    Check_Throw,
    Check_ThrowNew,
    Check_ExceptionOccurred,
    Check_ExceptionDescribe,
    Check_ExceptionClear,
    Check_FatalError,

    Check_PushLocalFrame,
    Check_PopLocalFrame,

    Check_NewGlobalRef,
    Check_DeleteGlobalRef,
    Check_DeleteLocalRef,
    Check_IsSameObject,
    Check_NewLocalRef,
    Check_EnsureLocalCapacity,

    Check_AllocObject,
    Check_NewObject,
    Check_NewObjectV,
    Check_NewObjectA,

    Check_GetObjectClass,
    Check_IsInstanceOf,

    Check_GetMethodID,

    Check_CallObjectMethod,
    Check_CallObjectMethodV,
    Check_CallObjectMethodA,
    Check_CallBooleanMethod,
    Check_CallBooleanMethodV,
    Check_CallBooleanMethodA,
    Check_CallByteMethod,
    Check_CallByteMethodV,
    Check_CallByteMethodA,
    Check_CallCharMethod,
    Check_CallCharMethodV,
    Check_CallCharMethodA,
    Check_CallShortMethod,
    Check_CallShortMethodV,
    Check_CallShortMethodA,
    Check_CallIntMethod,
    Check_CallIntMethodV,
    Check_CallIntMethodA,
    Check_CallLongMethod,
    Check_CallLongMethodV,
    Check_CallLongMethodA,
    Check_CallFloatMethod,
    Check_CallFloatMethodV,
    Check_CallFloatMethodA,
    Check_CallDoubleMethod,
    Check_CallDoubleMethodV,
    Check_CallDoubleMethodA,
    Check_CallVoidMethod,
    Check_CallVoidMethodV,
    Check_CallVoidMethodA,

    Check_CallNonvirtualObjectMethod,
    Check_CallNonvirtualObjectMethodV,
    Check_CallNonvirtualObjectMethodA,
    Check_CallNonvirtualBooleanMethod,
    Check_CallNonvirtualBooleanMethodV,
    Check_CallNonvirtualBooleanMethodA,
    Check_CallNonvirtualByteMethod,
    Check_CallNonvirtualByteMethodV,
    Check_CallNonvirtualByteMethodA,
    Check_CallNonvirtualCharMethod,
    Check_CallNonvirtualCharMethodV,
    Check_CallNonvirtualCharMethodA,
    Check_CallNonvirtualShortMethod,
    Check_CallNonvirtualShortMethodV,
    Check_CallNonvirtualShortMethodA,
    Check_CallNonvirtualIntMethod,
    Check_CallNonvirtualIntMethodV,
    Check_CallNonvirtualIntMethodA,
    Check_CallNonvirtualLongMethod,
    Check_CallNonvirtualLongMethodV,
    Check_CallNonvirtualLongMethodA,
    Check_CallNonvirtualFloatMethod,
    Check_CallNonvirtualFloatMethodV,
    Check_CallNonvirtualFloatMethodA,
    Check_CallNonvirtualDoubleMethod,
    Check_CallNonvirtualDoubleMethodV,
    Check_CallNonvirtualDoubleMethodA,
    Check_CallNonvirtualVoidMethod,
    Check_CallNonvirtualVoidMethodV,
    Check_CallNonvirtualVoidMethodA,

    Check_GetFieldID,

    Check_GetObjectField,
    Check_GetBooleanField,
    Check_GetByteField,
    Check_GetCharField,
    Check_GetShortField,
    Check_GetIntField,
    Check_GetLongField,
    Check_GetFloatField,
    Check_GetDoubleField,
    Check_SetObjectField,
    Check_SetBooleanField,
    Check_SetByteField,
    Check_SetCharField,
    Check_SetShortField,
    Check_SetIntField,
    Check_SetLongField,
    Check_SetFloatField,
    Check_SetDoubleField,

    Check_GetStaticMethodID,

    Check_CallStaticObjectMethod,
    Check_CallStaticObjectMethodV,
    Check_CallStaticObjectMethodA,
    Check_CallStaticBooleanMethod,
    Check_CallStaticBooleanMethodV,
    Check_CallStaticBooleanMethodA,
    Check_CallStaticByteMethod,
    Check_CallStaticByteMethodV,
    Check_CallStaticByteMethodA,
    Check_CallStaticCharMethod,
    Check_CallStaticCharMethodV,
    Check_CallStaticCharMethodA,
    Check_CallStaticShortMethod,
    Check_CallStaticShortMethodV,
    Check_CallStaticShortMethodA,
    Check_CallStaticIntMethod,
    Check_CallStaticIntMethodV,
    Check_CallStaticIntMethodA,
    Check_CallStaticLongMethod,
    Check_CallStaticLongMethodV,
    Check_CallStaticLongMethodA,
    Check_CallStaticFloatMethod,
    Check_CallStaticFloatMethodV,
    Check_CallStaticFloatMethodA,
    Check_CallStaticDoubleMethod,
    Check_CallStaticDoubleMethodV,
    Check_CallStaticDoubleMethodA,
    Check_CallStaticVoidMethod,
    Check_CallStaticVoidMethodV,
    Check_CallStaticVoidMethodA,

    Check_GetStaticFieldID,

    Check_GetStaticObjectField,
    Check_GetStaticBooleanField,
    Check_GetStaticByteField,
    Check_GetStaticCharField,
    Check_GetStaticShortField,
    Check_GetStaticIntField,
    Check_GetStaticLongField,
    Check_GetStaticFloatField,
    Check_GetStaticDoubleField,

    Check_SetStaticObjectField,
    Check_SetStaticBooleanField,
    Check_SetStaticByteField,
    Check_SetStaticCharField,
    Check_SetStaticShortField,
    Check_SetStaticIntField,
    Check_SetStaticLongField,
    Check_SetStaticFloatField,
    Check_SetStaticDoubleField,

    Check_NewString,

    Check_GetStringLength,
    Check_GetStringChars,
    Check_ReleaseStringChars,

    Check_NewStringUTF,
    Check_GetStringUTFLength,
    Check_GetStringUTFChars,
    Check_ReleaseStringUTFChars,

    Check_GetArrayLength,
    Check_NewObjectArray,
    Check_GetObjectArrayElement,
    Check_SetObjectArrayElement,

    Check_NewBooleanArray,
    Check_NewByteArray,
    Check_NewCharArray,
    Check_NewShortArray,
    Check_NewIntArray,
    Check_NewLongArray,
    Check_NewFloatArray,
    Check_NewDoubleArray,

    Check_GetBooleanArrayElements,
    Check_GetByteArrayElements,
    Check_GetCharArrayElements,
    Check_GetShortArrayElements,
    Check_GetIntArrayElements,
    Check_GetLongArrayElements,
    Check_GetFloatArrayElements,
    Check_GetDoubleArrayElements,

    Check_ReleaseBooleanArrayElements,
    Check_ReleaseByteArrayElements,
    Check_ReleaseCharArrayElements,
    Check_ReleaseShortArrayElements,
    Check_ReleaseIntArrayElements,
    Check_ReleaseLongArrayElements,
    Check_ReleaseFloatArrayElements,
    Check_ReleaseDoubleArrayElements,

    Check_GetBooleanArrayRegion,
    Check_GetByteArrayRegion,
    Check_GetCharArrayRegion,
    Check_GetShortArrayRegion,
    Check_GetIntArrayRegion,
    Check_GetLongArrayRegion,
    Check_GetFloatArrayRegion,
    Check_GetDoubleArrayRegion,
    Check_SetBooleanArrayRegion,
    Check_SetByteArrayRegion,
    Check_SetCharArrayRegion,
    Check_SetShortArrayRegion,
    Check_SetIntArrayRegion,
    Check_SetLongArrayRegion,
    Check_SetFloatArrayRegion,
    Check_SetDoubleArrayRegion,

    Check_RegisterNatives,
    Check_UnregisterNatives,

    Check_MonitorEnter,
    Check_MonitorExit,

    Check_GetJavaVM,

    Check_GetStringRegion,
    Check_GetStringUTFRegion,

    Check_GetPrimitiveArrayCritical,
    Check_ReleasePrimitiveArrayCritical,

    Check_GetStringCritical,
    Check_ReleaseStringCritical,

    Check_NewWeakGlobalRef,
    Check_DeleteWeakGlobalRef,

    Check_ExceptionCheck,

    Check_NewDirectByteBuffer,
    Check_GetDirectBufferAddress,
    Check_GetDirectBufferCapacity,

    Check_GetObjectRefType
};

static const struct JNIInvokeInterface gCheckInvokeInterface = {
    NULL,
    NULL,
    NULL,

    Check_DestroyJavaVM,
    Check_AttachCurrentThread,
    Check_DetachCurrentThread,

    Check_GetEnv,

    Check_AttachCurrentThreadAsDaemon,
};

/*
 * Replace the normal table with the checked table.
 */
void dvmUseCheckedJniEnv(JNIEnvExt* pEnv) {
    assert(pEnv->funcTable != &gCheckNativeInterface);
    pEnv->baseFuncTable = pEnv->funcTable;
    pEnv->funcTable = &gCheckNativeInterface;
}

/*
 * Replace the normal table with the checked table.
 */
void dvmUseCheckedJniVm(JavaVMExt* pVm) {
    assert(pVm->funcTable != &gCheckInvokeInterface);
    pVm->baseFuncTable = pVm->funcTable;
    pVm->funcTable = &gCheckInvokeInterface;
}
