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
 * Handle messages from debugger.
 *
 * GENERAL NOTE: we're not currently testing the message length for
 * correctness.  This is usually a bad idea, but here we can probably
 * get away with it so long as the debugger isn't broken.  We can
 * change the "read" macros to use "dataLen" to avoid wandering into
 * bad territory, and have a single "is dataLen correct" check at the
 * end of each function.  Not needed at this time.
 */
#include "jdwp/JdwpPriv.h"
#include "jdwp/JdwpHandler.h"
#include "jdwp/JdwpEvent.h"
#include "jdwp/JdwpConstants.h"
#include "jdwp/ExpandBuf.h"

#include "Bits.h"
#include "Atomic.h"
#include "DalvikVersion.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Helper function: read a "location" from an input buffer.
 */
static void jdwpReadLocation(const u1** pBuf, JdwpLocation* pLoc)
{
    memset(pLoc, 0, sizeof(*pLoc));     /* allows memcmp() later */
    pLoc->typeTag = read1(pBuf);
    pLoc->classId = dvmReadObjectId(pBuf);
    pLoc->methodId = dvmReadMethodId(pBuf);
    pLoc->idx = read8BE(pBuf);
}

/*
 * Helper function: write a "location" into the reply buffer.
 */
void dvmJdwpAddLocation(ExpandBuf* pReply, const JdwpLocation* pLoc)
{
    expandBufAdd1(pReply, pLoc->typeTag);
    expandBufAddObjectId(pReply, pLoc->classId);
    expandBufAddMethodId(pReply, pLoc->methodId);
    expandBufAdd8BE(pReply, pLoc->idx);
}

/*
 * Helper function: read a variable-width value from the input buffer.
 */
static u8 jdwpReadValue(const u1** pBuf, int width)
{
    u8 value;

    switch (width) {
    case 1:     value = read1(pBuf);                break;
    case 2:     value = read2BE(pBuf);              break;
    case 4:     value = read4BE(pBuf);              break;
    case 8:     value = read8BE(pBuf);              break;
    default:    value = (u8) -1; assert(false);     break;
    }

    return value;
}

/*
 * Helper function: write a variable-width value into the output input buffer.
 */
static void jdwpWriteValue(ExpandBuf* pReply, int width, u8 value)
{
    switch (width) {
    case 1:     expandBufAdd1(pReply, value);       break;
    case 2:     expandBufAdd2BE(pReply, value);     break;
    case 4:     expandBufAdd4BE(pReply, value);     break;
    case 8:     expandBufAdd8BE(pReply, value);     break;
    default:    assert(false);                      break;
    }
}

/*
 * Common code for *_InvokeMethod requests.
 *
 * If "isConstructor" is set, this returns "objectId" rather than the
 * expected-to-be-void return value of the called function.
 */
static JdwpError finishInvoke(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply,
    ObjectId threadId, ObjectId objectId, RefTypeId classId, MethodId methodId,
    bool isConstructor)
{
    assert(!isConstructor || objectId != 0);

    u4 numArgs = read4BE(&buf);

    ALOGV("    --> threadId=%llx objectId=%llx", threadId, objectId);
    ALOGV("        classId=%llx methodId=%x %s.%s",
        classId, methodId,
        dvmDbgGetClassDescriptor(classId),
        dvmDbgGetMethodName(classId, methodId));
    ALOGV("        %d args:", numArgs);

    u8* argArray = NULL;
    if (numArgs > 0)
        argArray = (ObjectId*) malloc(sizeof(ObjectId) * numArgs);

    for (u4 i = 0; i < numArgs; i++) {
        u1 typeTag = read1(&buf);
        int width = dvmDbgGetTagWidth(typeTag);
        u8 value = jdwpReadValue(&buf, width);

        ALOGV("          '%c'(%d): 0x%llx", typeTag, width, value);
        argArray[i] = value;
    }

    u4 options = read4BE(&buf);  /* enum InvokeOptions bit flags */
    ALOGV("        options=0x%04x%s%s", options,
        (options & INVOKE_SINGLE_THREADED) ? " (SINGLE_THREADED)" : "",
        (options & INVOKE_NONVIRTUAL) ? " (NONVIRTUAL)" : "");


    u1 resultTag;
    u8 resultValue;
    ObjectId exceptObjId;
    JdwpError err = dvmDbgInvokeMethod(threadId, objectId, classId, methodId,
            numArgs, argArray, options,
            &resultTag, &resultValue, &exceptObjId);
    if (err != ERR_NONE)
        goto bail;

    if (err == ERR_NONE) {
        if (isConstructor) {
            expandBufAdd1(pReply, JT_OBJECT);
            expandBufAddObjectId(pReply, objectId);
        } else {
            int width = dvmDbgGetTagWidth(resultTag);

            expandBufAdd1(pReply, resultTag);
            if (width != 0)
                jdwpWriteValue(pReply, width, resultValue);
        }
        expandBufAdd1(pReply, JT_OBJECT);
        expandBufAddObjectId(pReply, exceptObjId);

        ALOGV("  --> returned '%c' 0x%llx (except=%08llx)",
            resultTag, resultValue, exceptObjId);

        /* show detailed debug output */
        if (resultTag == JT_STRING && exceptObjId == 0) {
            if (resultValue != 0) {
                char* str = dvmDbgStringToUtf8(resultValue);
                ALOGV("      string '%s'", str);
                free(str);
            } else {
                ALOGV("      string (null)");
            }
        }
    }

bail:
    free(argArray);
    return err;
}


/*
 * Request for version info.
 */
static JdwpError handleVM_Version(JdwpState* state, const u1* buf,
    int dataLen, ExpandBuf* pReply)
{
    char tmpBuf[128];

    /* text information on VM version */
    sprintf(tmpBuf, "Android DalvikVM %d.%d.%d",
        DALVIK_MAJOR_VERSION, DALVIK_MINOR_VERSION, DALVIK_BUG_VERSION);
    expandBufAddUtf8String(pReply, (const u1*) tmpBuf);
    /* JDWP version numbers */
    expandBufAdd4BE(pReply, 1);        // major
    expandBufAdd4BE(pReply, 5);        // minor
    /* VM JRE version */
    expandBufAddUtf8String(pReply, (const u1*) "1.5.0");  /* e.g. 1.5.0_04 */
    /* target VM name */
    expandBufAddUtf8String(pReply, (const u1*) "DalvikVM");

    return ERR_NONE;
}

/*
 * Given a class JNI signature (e.g. "Ljava/lang/Error;"), return the
 * referenceTypeID.  We need to send back more than one if the class has
 * been loaded by multiple class loaders.
 */
static JdwpError handleVM_ClassesBySignature(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    size_t strLen;
    char* classDescriptor = readNewUtf8String(&buf, &strLen);
    ALOGV("  Req for class by signature '%s'", classDescriptor);

    /*
     * TODO: if a class with the same name has been loaded multiple times
     * (by different class loaders), we're supposed to return each of them.
     *
     * NOTE: this may mangle "className".
     */
    u4 numClasses;
    RefTypeId refTypeId;
    if (!dvmDbgFindLoadedClassBySignature(classDescriptor, &refTypeId)) {
        /* not currently loaded */
        ALOGV("    --> no match!");
        numClasses = 0;
    } else {
        /* just the one */
        numClasses = 1;
    }

    expandBufAdd4BE(pReply, numClasses);

    if (numClasses > 0) {
        u1 typeTag;
        u4 status;

        /* get class vs. interface and status flags */
        dvmDbgGetClassInfo(refTypeId, &typeTag, &status, NULL);

        expandBufAdd1(pReply, typeTag);
        expandBufAddRefTypeId(pReply, refTypeId);
        expandBufAdd4BE(pReply, status);
    }

    free(classDescriptor);

    return ERR_NONE;
}

/*
 * Handle request for the thread IDs of all running threads.
 *
 * We exclude ourselves from the list, because we don't allow ourselves
 * to be suspended, and that violates some JDWP expectations.
 */
static JdwpError handleVM_AllThreads(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId* pThreadIds;
    u4 threadCount;
    dvmDbgGetAllThreads(&pThreadIds, &threadCount);

    expandBufAdd4BE(pReply, threadCount);

    ObjectId* walker = pThreadIds;
    for (u4 i = 0; i < threadCount; i++) {
        expandBufAddObjectId(pReply, *walker++);
    }

    free(pThreadIds);

    return ERR_NONE;
}

/*
 * List all thread groups that do not have a parent.
 */
static JdwpError handleVM_TopLevelThreadGroups(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    /*
     * TODO: maintain a list of parentless thread groups in the VM.
     *
     * For now, just return "system".  Application threads are created
     * in "main", which is a child of "system".
     */
    u4 groups = 1;
    expandBufAdd4BE(pReply, groups);
    //threadGroupId = debugGetMainThreadGroup();
    //expandBufAdd8BE(pReply, threadGroupId);
    ObjectId threadGroupId = dvmDbgGetSystemThreadGroupId();
    expandBufAddObjectId(pReply, threadGroupId);

    return ERR_NONE;
}

/*
 * Respond with the sizes of the basic debugger types.
 *
 * All IDs are 8 bytes.
 */
static JdwpError handleVM_IDSizes(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    expandBufAdd4BE(pReply, sizeof(FieldId));
    expandBufAdd4BE(pReply, sizeof(MethodId));
    expandBufAdd4BE(pReply, sizeof(ObjectId));
    expandBufAdd4BE(pReply, sizeof(RefTypeId));
    expandBufAdd4BE(pReply, sizeof(FrameId));
    return ERR_NONE;
}

/*
 * The debugger is politely asking to disconnect.  We're good with that.
 *
 * We could resume threads and clean up pinned references, but we can do
 * that when the TCP connection drops.
 */
static JdwpError handleVM_Dispose(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    return ERR_NONE;
}

/*
 * Suspend the execution of the application running in the VM (i.e. suspend
 * all threads).
 *
 * This needs to increment the "suspend count" on all threads.
 */
static JdwpError handleVM_Suspend(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    dvmDbgSuspendVM(false);
    return ERR_NONE;
}

/*
 * Resume execution.  Decrements the "suspend count" of all threads.
 */
static JdwpError handleVM_Resume(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    dvmDbgResumeVM();
    return ERR_NONE;
}

/*
 * The debugger wants the entire VM to exit.
 */
static JdwpError handleVM_Exit(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    u4 exitCode = get4BE(buf);

    ALOGW("Debugger is telling the VM to exit with code=%d", exitCode);

    dvmDbgExit(exitCode);
    return ERR_NOT_IMPLEMENTED;     // shouldn't get here
}

/*
 * Create a new string in the VM and return its ID.
 *
 * (Ctrl-Shift-I in Eclipse on an array of objects causes it to create the
 * string "java.util.Arrays".)
 */
static JdwpError handleVM_CreateString(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    size_t strLen;
    char* str = readNewUtf8String(&buf, &strLen);

    ALOGV("  Req to create string '%s'", str);

    ObjectId stringId = dvmDbgCreateString(str);
    free(str);
    if (stringId == 0)
        return ERR_OUT_OF_MEMORY;

    expandBufAddObjectId(pReply, stringId);
    return ERR_NONE;
}

/*
 * Tell the debugger what we are capable of.
 */
static JdwpError handleVM_Capabilities(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    expandBufAdd1(pReply, false);   /* canWatchFieldModification */
    expandBufAdd1(pReply, false);   /* canWatchFieldAccess */
    expandBufAdd1(pReply, false);   /* canGetBytecodes */
    expandBufAdd1(pReply, true);    /* canGetSyntheticAttribute */
    expandBufAdd1(pReply, false);   /* canGetOwnedMonitorInfo */
    expandBufAdd1(pReply, false);   /* canGetCurrentContendedMonitor */
    expandBufAdd1(pReply, false);   /* canGetMonitorInfo */
    return ERR_NONE;
}

/*
 * Return classpath and bootclasspath.
 */
static JdwpError handleVM_ClassPaths(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    char baseDir[2] = "/";

    /*
     * TODO: make this real.  Not important for remote debugging, but
     * might be useful for local debugging.
     */
    u4 classPaths = 1;
    u4 bootClassPaths = 0;

    expandBufAddUtf8String(pReply, (const u1*) baseDir);
    expandBufAdd4BE(pReply, classPaths);
    for (u4 i = 0; i < classPaths; i++) {
        expandBufAddUtf8String(pReply, (const u1*) ".");
    }

    expandBufAdd4BE(pReply, bootClassPaths);
    for (u4 i = 0; i < classPaths; i++) {
        /* add bootclasspath components as strings */
    }

    return ERR_NONE;
}

/*
 * Release a list of object IDs.  (Seen in jdb.)
 *
 * Currently does nothing.
 */
static JdwpError HandleVM_DisposeObjects(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    return ERR_NONE;
}

/*
 * Tell the debugger what we are capable of.
 */
static JdwpError handleVM_CapabilitiesNew(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    expandBufAdd1(pReply, false);   /* canWatchFieldModification */
    expandBufAdd1(pReply, false);   /* canWatchFieldAccess */
    expandBufAdd1(pReply, false);   /* canGetBytecodes */
    expandBufAdd1(pReply, true);    /* canGetSyntheticAttribute */
    expandBufAdd1(pReply, false);   /* canGetOwnedMonitorInfo */
    expandBufAdd1(pReply, false);   /* canGetCurrentContendedMonitor */
    expandBufAdd1(pReply, false);   /* canGetMonitorInfo */
    expandBufAdd1(pReply, false);   /* canRedefineClasses */
    expandBufAdd1(pReply, false);   /* canAddMethod */
    expandBufAdd1(pReply, false);   /* canUnrestrictedlyRedefineClasses */
    expandBufAdd1(pReply, false);   /* canPopFrames */
    expandBufAdd1(pReply, false);   /* canUseInstanceFilters */
    expandBufAdd1(pReply, false);   /* canGetSourceDebugExtension */
    expandBufAdd1(pReply, false);   /* canRequestVMDeathEvent */
    expandBufAdd1(pReply, false);   /* canSetDefaultStratum */
    expandBufAdd1(pReply, false);   /* 1.6: canGetInstanceInfo */
    expandBufAdd1(pReply, false);   /* 1.6: canRequestMonitorEvents */
    expandBufAdd1(pReply, false);   /* 1.6: canGetMonitorFrameInfo */
    expandBufAdd1(pReply, false);   /* 1.6: canUseSourceNameFilters */
    expandBufAdd1(pReply, false);   /* 1.6: canGetConstantPool */
    expandBufAdd1(pReply, false);   /* 1.6: canForceEarlyReturn */

    /* fill in reserved22 through reserved32; note count started at 1 */
    for (int i = 22; i <= 32; i++)
        expandBufAdd1(pReply, false);   /* reservedN */
    return ERR_NONE;
}

/*
 * Cough up the complete list of classes.
 */
static JdwpError handleVM_AllClassesWithGeneric(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    u4 numClasses = 0;
    RefTypeId* classRefBuf = NULL;

    dvmDbgGetClassList(&numClasses, &classRefBuf);

    expandBufAdd4BE(pReply, numClasses);

    for (u4 i = 0; i < numClasses; i++) {
        static const u1 genericSignature[1] = "";
        u1 refTypeTag;
        const char* signature;
        u4 status;

        dvmDbgGetClassInfo(classRefBuf[i], &refTypeTag, &status, &signature);

        expandBufAdd1(pReply, refTypeTag);
        expandBufAddRefTypeId(pReply, classRefBuf[i]);
        expandBufAddUtf8String(pReply, (const u1*) signature);
        expandBufAddUtf8String(pReply, genericSignature);
        expandBufAdd4BE(pReply, status);
    }

    free(classRefBuf);

    return ERR_NONE;
}

/*
 * Given a referenceTypeID, return a string with the JNI reference type
 * signature (e.g. "Ljava/lang/Error;").
 */
static JdwpError handleRT_Signature(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId refTypeId = dvmReadRefTypeId(&buf);

    ALOGV("  Req for signature of refTypeId=0x%llx", refTypeId);
    const char* signature = dvmDbgGetSignature(refTypeId);
    expandBufAddUtf8String(pReply, (const u1*) signature);

    return ERR_NONE;
}

/*
 * Return the modifiers (a/k/a access flags) for a reference type.
 */
static JdwpError handleRT_Modifiers(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId refTypeId = dvmReadRefTypeId(&buf);
    u4 modBits = dvmDbgGetAccessFlags(refTypeId);

    expandBufAdd4BE(pReply, modBits);

    return ERR_NONE;
}

/*
 * Get values from static fields in a reference type.
 */
static JdwpError handleRT_GetValues(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId refTypeId = dvmReadRefTypeId(&buf);
    u4 numFields = read4BE(&buf);

    ALOGV("  RT_GetValues %u:", numFields);

    expandBufAdd4BE(pReply, numFields);
    for (u4 i = 0; i < numFields; i++) {
        FieldId fieldId = dvmReadFieldId(&buf);
        dvmDbgGetStaticFieldValue(refTypeId, fieldId, pReply);
    }

    return ERR_NONE;
}

/*
 * Get the name of the source file in which a reference type was declared.
 */
static JdwpError handleRT_SourceFile(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId refTypeId = dvmReadRefTypeId(&buf);

    const char* fileName = dvmDbgGetSourceFile(refTypeId);
    if (fileName != NULL) {
        expandBufAddUtf8String(pReply, (const u1*) fileName);
        return ERR_NONE;
    } else {
        return ERR_ABSENT_INFORMATION;
    }
}

/*
 * Return the current status of the reference type.
 */
static JdwpError handleRT_Status(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId refTypeId = dvmReadRefTypeId(&buf);

    /* get status flags */
    u1 typeTag;
    u4 status;
    dvmDbgGetClassInfo(refTypeId, &typeTag, &status, NULL);
    expandBufAdd4BE(pReply, status);
    return ERR_NONE;
}

/*
 * Return interfaces implemented directly by this class.
 */
static JdwpError handleRT_Interfaces(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId refTypeId = dvmReadRefTypeId(&buf);

    ALOGV("  Req for interfaces in %llx (%s)", refTypeId,
        dvmDbgGetClassDescriptor(refTypeId));

    dvmDbgOutputAllInterfaces(refTypeId, pReply);

    return ERR_NONE;
}

/*
 * Return the class object corresponding to this type.
 */
static JdwpError handleRT_ClassObject(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId refTypeId = dvmReadRefTypeId(&buf);
    ObjectId classObjId = dvmDbgGetClassObject(refTypeId);

    ALOGV("  RefTypeId %llx -> ObjectId %llx", refTypeId, classObjId);

    expandBufAddObjectId(pReply, classObjId);

    return ERR_NONE;
}

/*
 * Returns the value of the SourceDebugExtension attribute.
 *
 * JDB seems interested, but DEX files don't currently support this.
 */
static JdwpError handleRT_SourceDebugExtension(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    /* referenceTypeId in, string out */
    return ERR_ABSENT_INFORMATION;
}

/*
 * Like RT_Signature but with the possibility of a "generic signature".
 */
static JdwpError handleRT_SignatureWithGeneric(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    static const u1 genericSignature[1] = "";

    RefTypeId refTypeId = dvmReadRefTypeId(&buf);

    ALOGV("  Req for signature of refTypeId=0x%llx", refTypeId);
    const char* signature = dvmDbgGetSignature(refTypeId);
    if (signature != NULL) {
        expandBufAddUtf8String(pReply, (const u1*) signature);
    } else {
        ALOGW("No signature for refTypeId=0x%llx", refTypeId);
        expandBufAddUtf8String(pReply, (const u1*) "Lunknown;");
    }
    expandBufAddUtf8String(pReply, genericSignature);

    return ERR_NONE;
}

/*
 * Return the instance of java.lang.ClassLoader that loaded the specified
 * reference type, or null if it was loaded by the system loader.
 */
static JdwpError handleRT_ClassLoader(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId refTypeId = dvmReadRefTypeId(&buf);

    expandBufAddObjectId(pReply, dvmDbgGetClassLoader(refTypeId));

    return ERR_NONE;
}

/*
 * Given a referenceTypeId, return a block of stuff that describes the
 * fields declared by a class.
 */
static JdwpError handleRT_FieldsWithGeneric(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId refTypeId = dvmReadRefTypeId(&buf);
    ALOGV("  Req for fields in refTypeId=0x%llx", refTypeId);
    ALOGV("  --> '%s'", dvmDbgGetSignature(refTypeId));

    dvmDbgOutputAllFields(refTypeId, true, pReply);

    return ERR_NONE;
}

/*
 * Given a referenceTypeID, return a block of goodies describing the
 * methods declared by a class.
 */
static JdwpError handleRT_MethodsWithGeneric(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId refTypeId = dvmReadRefTypeId(&buf);

    ALOGV("  Req for methods in refTypeId=0x%llx", refTypeId);
    ALOGV("  --> '%s'", dvmDbgGetSignature(refTypeId));

    dvmDbgOutputAllMethods(refTypeId, true, pReply);

    return ERR_NONE;
}

/*
 * Return the immediate superclass of a class.
 */
static JdwpError handleCT_Superclass(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId classId = dvmReadRefTypeId(&buf);

    RefTypeId superClassId = dvmDbgGetSuperclass(classId);

    expandBufAddRefTypeId(pReply, superClassId);

    return ERR_NONE;
}

/*
 * Set static class values.
 */
static JdwpError handleCT_SetValues(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId classId = dvmReadRefTypeId(&buf);
    u4 values = read4BE(&buf);

    ALOGV("  Req to set %d values in classId=%llx", values, classId);

    for (u4 i = 0; i < values; i++) {
        FieldId fieldId = dvmReadFieldId(&buf);
        u1 fieldTag = dvmDbgGetStaticFieldBasicTag(classId, fieldId);
        int width = dvmDbgGetTagWidth(fieldTag);
        u8 value = jdwpReadValue(&buf, width);

        ALOGV("    --> field=%x tag=%c -> %lld", fieldId, fieldTag, value);
        dvmDbgSetStaticFieldValue(classId, fieldId, value, width);
    }

    return ERR_NONE;
}

/*
 * Invoke a static method.
 *
 * Example: Eclipse sometimes uses java/lang/Class.forName(String s) on
 * values in the "variables" display.
 */
static JdwpError handleCT_InvokeMethod(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId classId = dvmReadRefTypeId(&buf);
    ObjectId threadId = dvmReadObjectId(&buf);
    MethodId methodId = dvmReadMethodId(&buf);

    return finishInvoke(state, buf, dataLen, pReply,
            threadId, 0, classId, methodId, false);
}

/*
 * Create a new object of the requested type, and invoke the specified
 * constructor.
 *
 * Example: in IntelliJ, create a watch on "new String(myByteArray)" to
 * see the contents of a byte[] as a string.
 */
static JdwpError handleCT_NewInstance(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId classId = dvmReadRefTypeId(&buf);
    ObjectId threadId = dvmReadObjectId(&buf);
    MethodId methodId = dvmReadMethodId(&buf);

    ALOGV("Creating instance of %s", dvmDbgGetClassDescriptor(classId));
    ObjectId objectId = dvmDbgCreateObject(classId);
    if (objectId == 0)
        return ERR_OUT_OF_MEMORY;

    return finishInvoke(state, buf, dataLen, pReply,
            threadId, objectId, classId, methodId, true);
}

/*
 * Create a new array object of the requested type and length.
 */
static JdwpError handleAT_newInstance(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId arrayTypeId = dvmReadRefTypeId(&buf);
    u4 length = read4BE(&buf);

    ALOGV("Creating array %s[%u]",
        dvmDbgGetClassDescriptor(arrayTypeId), length);
    ObjectId objectId = dvmDbgCreateArrayObject(arrayTypeId, length);
    if (objectId == 0)
        return ERR_OUT_OF_MEMORY;

    expandBufAdd1(pReply, JT_ARRAY);
    expandBufAddObjectId(pReply, objectId);
    return ERR_NONE;
}

/*
 * Return line number information for the method, if present.
 */
static JdwpError handleM_LineTable(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId refTypeId = dvmReadRefTypeId(&buf);
    MethodId methodId = dvmReadMethodId(&buf);

    ALOGV("  Req for line table in %s.%s",
        dvmDbgGetClassDescriptor(refTypeId),
        dvmDbgGetMethodName(refTypeId,methodId));

    dvmDbgOutputLineTable(refTypeId, methodId, pReply);

    return ERR_NONE;
}

/*
 * Pull out the LocalVariableTable goodies.
 */
static JdwpError handleM_VariableTableWithGeneric(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId classId = dvmReadRefTypeId(&buf);
    MethodId methodId = dvmReadMethodId(&buf);

    ALOGV("  Req for LocalVarTab in class=%s method=%s",
        dvmDbgGetClassDescriptor(classId),
        dvmDbgGetMethodName(classId, methodId));

    /*
     * We could return ERR_ABSENT_INFORMATION here if the DEX file was
     * built without local variable information.  That will cause Eclipse
     * to make a best-effort attempt at displaying local variables
     * anonymously.  However, the attempt isn't very good, so we're probably
     * better off just not showing anything.
     */
    dvmDbgOutputVariableTable(classId, methodId, true, pReply);
    return ERR_NONE;
}

/*
 * Given an object reference, return the runtime type of the object
 * (class or array).
 *
 * This can get called on different things, e.g. threadId gets
 * passed in here.
 */
static JdwpError handleOR_ReferenceType(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId objectId = dvmReadObjectId(&buf);
    ALOGV("  Req for type of objectId=0x%llx", objectId);

    u1 refTypeTag;
    RefTypeId typeId;
    dvmDbgGetObjectType(objectId, &refTypeTag, &typeId);

    expandBufAdd1(pReply, refTypeTag);
    expandBufAddRefTypeId(pReply, typeId);

    return ERR_NONE;
}

/*
 * Get values from the fields of an object.
 */
static JdwpError handleOR_GetValues(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId objectId = dvmReadObjectId(&buf);
    u4 numFields = read4BE(&buf);

    ALOGV("  Req for %d fields from objectId=0x%llx", numFields, objectId);

    expandBufAdd4BE(pReply, numFields);

    for (u4 i = 0; i < numFields; i++) {
        FieldId fieldId = dvmReadFieldId(&buf);
        dvmDbgGetFieldValue(objectId, fieldId, pReply);
    }

    return ERR_NONE;
}

/*
 * Set values in the fields of an object.
 */
static JdwpError handleOR_SetValues(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId objectId = dvmReadObjectId(&buf);
    u4 numFields = read4BE(&buf);

    ALOGV("  Req to set %d fields in objectId=0x%llx", numFields, objectId);

    for (u4 i = 0; i < numFields; i++) {
        FieldId fieldId = dvmReadFieldId(&buf);

        u1 fieldTag = dvmDbgGetFieldBasicTag(objectId, fieldId);
        int width = dvmDbgGetTagWidth(fieldTag);
        u8 value = jdwpReadValue(&buf, width);

        ALOGV("    --> fieldId=%x tag='%c'(%d) value=%lld",
            fieldId, fieldTag, width, value);

        dvmDbgSetFieldValue(objectId, fieldId, value, width);
    }

    return ERR_NONE;
}

/*
 * Invoke an instance method.  The invocation must occur in the specified
 * thread, which must have been suspended by an event.
 *
 * The call is synchronous.  All threads in the VM are resumed, unless the
 * SINGLE_THREADED flag is set.
 *
 * If you ask Eclipse to "inspect" an object (or ask JDB to "print" an
 * object), it will try to invoke the object's toString() function.  This
 * feature becomes crucial when examining ArrayLists with Eclipse.
 */
static JdwpError handleOR_InvokeMethod(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId objectId = dvmReadObjectId(&buf);
    ObjectId threadId = dvmReadObjectId(&buf);
    RefTypeId classId = dvmReadRefTypeId(&buf);
    MethodId methodId = dvmReadMethodId(&buf);

    return finishInvoke(state, buf, dataLen, pReply,
            threadId, objectId, classId, methodId, false);
}

/*
 * Disable garbage collection of the specified object.
 */
static JdwpError handleOR_DisableCollection(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    // this is currently a no-op
    return ERR_NONE;
}

/*
 * Enable garbage collection of the specified object.
 */
static JdwpError handleOR_EnableCollection(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    // this is currently a no-op
    return ERR_NONE;
}

/*
 * Determine whether an object has been garbage collected.
 */
static JdwpError handleOR_IsCollected(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId objectId;

    objectId = dvmReadObjectId(&buf);
    ALOGV("  Req IsCollected(0x%llx)", objectId);

    // TODO: currently returning false; must integrate with GC
    expandBufAdd1(pReply, 0);

    return ERR_NONE;
}

/*
 * Return the string value in a string object.
 */
static JdwpError handleSR_Value(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId stringObject = dvmReadObjectId(&buf);
    char* str = dvmDbgStringToUtf8(stringObject);

    ALOGV("  Req for str %llx --> '%s'", stringObject, str);

    expandBufAddUtf8String(pReply, (u1*) str);
    free(str);

    return ERR_NONE;
}

/*
 * Return a thread's name.
 */
static JdwpError handleTR_Name(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadId = dvmReadObjectId(&buf);

    ALOGV("  Req for name of thread 0x%llx", threadId);
    char* name = dvmDbgGetThreadName(threadId);
    if (name == NULL)
        return ERR_INVALID_THREAD;

    expandBufAddUtf8String(pReply, (u1*) name);
    free(name);

    return ERR_NONE;
}

/*
 * Suspend the specified thread.
 *
 * It's supposed to remain suspended even if interpreted code wants to
 * resume it; only the JDI is allowed to resume it.
 */
static JdwpError handleTR_Suspend(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadId = dvmReadObjectId(&buf);

    if (threadId == dvmDbgGetThreadSelfId()) {
        ALOGI("  Warning: ignoring request to suspend self");
        return ERR_THREAD_NOT_SUSPENDED;
    }
    ALOGV("  Req to suspend thread 0x%llx", threadId);

    dvmDbgSuspendThread(threadId);

    return ERR_NONE;
}

/*
 * Resume the specified thread.
 */
static JdwpError handleTR_Resume(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadId = dvmReadObjectId(&buf);

    if (threadId == dvmDbgGetThreadSelfId()) {
        ALOGI("  Warning: ignoring request to resume self");
        return ERR_NONE;
    }
    ALOGV("  Req to resume thread 0x%llx", threadId);

    dvmDbgResumeThread(threadId);

    return ERR_NONE;
}

/*
 * Return status of specified thread.
 */
static JdwpError handleTR_Status(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadId = dvmReadObjectId(&buf);

    ALOGV("  Req for status of thread 0x%llx", threadId);

    u4 threadStatus;
    u4 suspendStatus;
    if (!dvmDbgGetThreadStatus(threadId, &threadStatus, &suspendStatus))
        return ERR_INVALID_THREAD;

    ALOGV("    --> %s, %s",
        dvmJdwpThreadStatusStr((JdwpThreadStatus) threadStatus),
        dvmJdwpSuspendStatusStr((JdwpSuspendStatus) suspendStatus));

    expandBufAdd4BE(pReply, threadStatus);
    expandBufAdd4BE(pReply, suspendStatus);

    return ERR_NONE;
}

/*
 * Return the thread group that the specified thread is a member of.
 */
static JdwpError handleTR_ThreadGroup(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadId = dvmReadObjectId(&buf);

    /* currently not handling these */
    ObjectId threadGroupId = dvmDbgGetThreadGroup(threadId);
    expandBufAddObjectId(pReply, threadGroupId);

    return ERR_NONE;
}

/*
 * Return the current call stack of a suspended thread.
 *
 * If the thread isn't suspended, the error code isn't defined, but should
 * be THREAD_NOT_SUSPENDED.
 */
static JdwpError handleTR_Frames(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadId = dvmReadObjectId(&buf);
    u4 startFrame = read4BE(&buf);
    u4 length = read4BE(&buf);

    if (!dvmDbgThreadExists(threadId))
        return ERR_INVALID_THREAD;
    if (!dvmDbgIsSuspended(threadId)) {
        ALOGV("  Rejecting req for frames in running thread '%s' (%llx)",
            dvmDbgGetThreadName(threadId), threadId);
        return ERR_THREAD_NOT_SUSPENDED;
    }

    int frameCount = dvmDbgGetThreadFrameCount(threadId);

    ALOGV("  Request for frames: threadId=%llx start=%d length=%d [count=%d]",
        threadId, startFrame, length, frameCount);
    if (frameCount <= 0)
        return ERR_THREAD_NOT_SUSPENDED;    /* == 0 means 100% native */

    if (length == (u4) -1)
        length = frameCount;
    assert((int) startFrame >= 0 && (int) startFrame < frameCount);
    assert((int) (startFrame + length) <= frameCount);

    u4 frames = length;
    expandBufAdd4BE(pReply, frames);
    for (u4 i = startFrame; i < (startFrame+length); i++) {
        FrameId frameId;
        JdwpLocation loc;

        dvmDbgGetThreadFrame(threadId, i, &frameId, &loc);

        expandBufAdd8BE(pReply, frameId);
        dvmJdwpAddLocation(pReply, &loc);

        LOGVV("    Frame %d: id=%llx loc={type=%d cls=%llx mth=%x loc=%llx}",
            i, frameId, loc.typeTag, loc.classId, loc.methodId, loc.idx);
    }

    return ERR_NONE;
}

/*
 * Returns the #of frames on the specified thread, which must be suspended.
 */
static JdwpError handleTR_FrameCount(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadId = dvmReadObjectId(&buf);

    if (!dvmDbgThreadExists(threadId))
        return ERR_INVALID_THREAD;
    if (!dvmDbgIsSuspended(threadId)) {
        ALOGV("  Rejecting req for frames in running thread '%s' (%llx)",
            dvmDbgGetThreadName(threadId), threadId);
        return ERR_THREAD_NOT_SUSPENDED;
    }

    int frameCount = dvmDbgGetThreadFrameCount(threadId);
    if (frameCount < 0)
        return ERR_INVALID_THREAD;
    expandBufAdd4BE(pReply, (u4)frameCount);

    return ERR_NONE;
}

/*
 * Get the monitor that the thread is waiting on.
 */
static JdwpError handleTR_CurrentContendedMonitor(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadId;

    threadId = dvmReadObjectId(&buf);

    // TODO: create an Object to represent the monitor (we're currently
    // just using a raw Monitor struct in the VM)

    return ERR_NOT_IMPLEMENTED;
}

/*
 * Return the suspend count for the specified thread.
 *
 * (The thread *might* still be running -- it might not have examined
 * its suspend count recently.)
 */
static JdwpError handleTR_SuspendCount(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadId = dvmReadObjectId(&buf);

    u4 suspendCount = dvmDbgGetThreadSuspendCount(threadId);
    expandBufAdd4BE(pReply, suspendCount);

    return ERR_NONE;
}

/*
 * Return the name of a thread group.
 *
 * The Eclipse debugger recognizes "main" and "system" as special.
 */
static JdwpError handleTGR_Name(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadGroupId = dvmReadObjectId(&buf);
    ALOGV("  Req for name of threadGroupId=0x%llx", threadGroupId);

    char* name = dvmDbgGetThreadGroupName(threadGroupId);
    if (name != NULL)
        expandBufAddUtf8String(pReply, (u1*) name);
    else {
        expandBufAddUtf8String(pReply, (u1*) "BAD-GROUP-ID");
        ALOGW("bad thread group ID");
    }

    free(name);

    return ERR_NONE;
}

/*
 * Returns the thread group -- if any -- that contains the specified
 * thread group.
 */
static JdwpError handleTGR_Parent(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId groupId = dvmReadObjectId(&buf);

    ObjectId parentGroup = dvmDbgGetThreadGroupParent(groupId);
    expandBufAddObjectId(pReply, parentGroup);

    return ERR_NONE;
}

/*
 * Return the active threads and thread groups that are part of the
 * specified thread group.
 */
static JdwpError handleTGR_Children(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadGroupId = dvmReadObjectId(&buf);
    ALOGV("  Req for threads in threadGroupId=0x%llx", threadGroupId);

    ObjectId* pThreadIds;
    u4 threadCount;
    dvmDbgGetThreadGroupThreads(threadGroupId, &pThreadIds, &threadCount);

    expandBufAdd4BE(pReply, threadCount);

    for (u4 i = 0; i < threadCount; i++)
        expandBufAddObjectId(pReply, pThreadIds[i]);
    free(pThreadIds);

    /*
     * TODO: finish support for child groups
     *
     * For now, just show that "main" is a child of "system".
     */
    if (threadGroupId == dvmDbgGetSystemThreadGroupId()) {
        expandBufAdd4BE(pReply, 1);
        expandBufAddObjectId(pReply, dvmDbgGetMainThreadGroupId());
    } else {
        expandBufAdd4BE(pReply, 0);
    }

    return ERR_NONE;
}

/*
 * Return the #of components in the array.
 */
static JdwpError handleAR_Length(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId arrayId = dvmReadObjectId(&buf);
    ALOGV("  Req for length of array 0x%llx", arrayId);

    u4 arrayLength = dvmDbgGetArrayLength(arrayId);

    ALOGV("    --> %d", arrayLength);

    expandBufAdd4BE(pReply, arrayLength);

    return ERR_NONE;
}

/*
 * Return the values from an array.
 */
static JdwpError handleAR_GetValues(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId arrayId = dvmReadObjectId(&buf);
    u4 firstIndex = read4BE(&buf);
    u4 length = read4BE(&buf);

    u1 tag = dvmDbgGetArrayElementTag(arrayId);
    ALOGV("  Req for array values 0x%llx first=%d len=%d (elem tag=%c)",
        arrayId, firstIndex, length, tag);

    expandBufAdd1(pReply, tag);
    expandBufAdd4BE(pReply, length);

    if (!dvmDbgOutputArray(arrayId, firstIndex, length, pReply))
        return ERR_INVALID_LENGTH;

    return ERR_NONE;
}

/*
 * Set values in an array.
 */
static JdwpError handleAR_SetValues(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId arrayId = dvmReadObjectId(&buf);
    u4 firstIndex = read4BE(&buf);
    u4 values = read4BE(&buf);

    ALOGV("  Req to set array values 0x%llx first=%d count=%d",
        arrayId, firstIndex, values);

    if (!dvmDbgSetArrayElements(arrayId, firstIndex, values, buf))
        return ERR_INVALID_LENGTH;

    return ERR_NONE;
}

/*
 * Return the set of classes visible to a class loader.  All classes which
 * have the class loader as a defining or initiating loader are returned.
 */
static JdwpError handleCLR_VisibleClasses(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId classLoaderObject;
    u4 numClasses = 0;
    RefTypeId* classRefBuf = NULL;
    int i;

    classLoaderObject = dvmReadObjectId(&buf);

    dvmDbgGetVisibleClassList(classLoaderObject, &numClasses, &classRefBuf);

    expandBufAdd4BE(pReply, numClasses);
    for (i = 0; i < (int) numClasses; i++) {
        u1 refTypeTag;

        refTypeTag = dvmDbgGetClassObjectType(classRefBuf[i]);

        expandBufAdd1(pReply, refTypeTag);
        expandBufAddRefTypeId(pReply, classRefBuf[i]);
    }

    return ERR_NONE;
}

/*
 * Set an event trigger.
 *
 * Reply with a requestID.
 */
static JdwpError handleER_Set(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    const u1* origBuf = buf;

    u1 eventKind = read1(&buf);
    u1 suspendPolicy = read1(&buf);
    u4 modifierCount = read4BE(&buf);

    LOGVV("  Set(kind=%s(%u) suspend=%s(%u) mods=%u)",
        dvmJdwpEventKindStr(eventKind), eventKind,
        dvmJdwpSuspendPolicyStr(suspendPolicy), suspendPolicy,
        modifierCount);

    assert(modifierCount < 256);    /* reasonableness check */

    JdwpEvent* pEvent = dvmJdwpEventAlloc(modifierCount);
    pEvent->eventKind = static_cast<JdwpEventKind>(eventKind);
    pEvent->suspendPolicy = static_cast<JdwpSuspendPolicy>(suspendPolicy);
    pEvent->modCount = modifierCount;

    /*
     * Read modifiers.  Ordering may be significant (see explanation of Count
     * mods in JDWP doc).
     */
    for (u4 idx = 0; idx < modifierCount; idx++) {
        u1 modKind = read1(&buf);

        pEvent->mods[idx].modKind = modKind;

        switch (modKind) {
        case MK_COUNT:          /* report once, when "--count" reaches 0 */
            {
                u4 count = read4BE(&buf);
                LOGVV("    Count: %u", count);
                if (count == 0)
                    return ERR_INVALID_COUNT;
                pEvent->mods[idx].count.count = count;
            }
            break;
        case MK_CONDITIONAL:    /* conditional on expression) */
            {
                u4 exprId = read4BE(&buf);
                LOGVV("    Conditional: %d", exprId);
                pEvent->mods[idx].conditional.exprId = exprId;
            }
            break;
        case MK_THREAD_ONLY:    /* only report events in specified thread */
            {
                ObjectId threadId = dvmReadObjectId(&buf);
                LOGVV("    ThreadOnly: %llx", threadId);
                pEvent->mods[idx].threadOnly.threadId = threadId;
            }
            break;
        case MK_CLASS_ONLY:     /* for ClassPrepare, MethodEntry */
            {
                RefTypeId clazzId = dvmReadRefTypeId(&buf);
                LOGVV("    ClassOnly: %llx (%s)",
                    clazzId, dvmDbgGetClassDescriptor(clazzId));
                pEvent->mods[idx].classOnly.refTypeId = clazzId;
            }
            break;
        case MK_CLASS_MATCH:    /* restrict events to matching classes */
            {
                char* pattern;
                size_t strLen;

                pattern = readNewUtf8String(&buf, &strLen);
                LOGVV("    ClassMatch: '%s'", pattern);
                /* pattern is "java.foo.*", we want "java/foo/ *" */
                pEvent->mods[idx].classMatch.classPattern =
                    dvmDotToSlash(pattern);
                free(pattern);
            }
            break;
        case MK_CLASS_EXCLUDE:  /* restrict events to non-matching classes */
            {
                char* pattern;
                size_t strLen;

                pattern = readNewUtf8String(&buf, &strLen);
                LOGVV("    ClassExclude: '%s'", pattern);
                pEvent->mods[idx].classExclude.classPattern =
                    dvmDotToSlash(pattern);
                free(pattern);
            }
            break;
        case MK_LOCATION_ONLY:  /* restrict certain events based on loc */
            {
                JdwpLocation loc;

                jdwpReadLocation(&buf, &loc);
                LOGVV("    LocationOnly: typeTag=%d classId=%llx methodId=%x idx=%llx",
                    loc.typeTag, loc.classId, loc.methodId, loc.idx);
                pEvent->mods[idx].locationOnly.loc = loc;
            }
            break;
        case MK_EXCEPTION_ONLY: /* modifies EK_EXCEPTION events */
            {
                RefTypeId exceptionOrNull;      /* null == all exceptions */
                u1 caught, uncaught;

                exceptionOrNull = dvmReadRefTypeId(&buf);
                caught = read1(&buf);
                uncaught = read1(&buf);
                LOGVV("    ExceptionOnly: type=%llx(%s) caught=%d uncaught=%d",
                    exceptionOrNull, (exceptionOrNull == 0) ? "null"
                        : dvmDbgGetClassDescriptor(exceptionOrNull),
                    caught, uncaught);

                pEvent->mods[idx].exceptionOnly.refTypeId = exceptionOrNull;
                pEvent->mods[idx].exceptionOnly.caught = caught;
                pEvent->mods[idx].exceptionOnly.uncaught = uncaught;
            }
            break;
        case MK_FIELD_ONLY:     /* for field access/mod events */
            {
                RefTypeId declaring = dvmReadRefTypeId(&buf);
                FieldId fieldId = dvmReadFieldId(&buf);
                LOGVV("    FieldOnly: %llx %x", declaring, fieldId);
                pEvent->mods[idx].fieldOnly.refTypeId = declaring;
                pEvent->mods[idx].fieldOnly.fieldId = fieldId;
            }
            break;
        case MK_STEP:           /* for use with EK_SINGLE_STEP */
            {
                ObjectId threadId;
                u4 size, depth;

                threadId = dvmReadObjectId(&buf);
                size = read4BE(&buf);
                depth = read4BE(&buf);
                LOGVV("    Step: thread=%llx size=%s depth=%s",
                    threadId, dvmJdwpStepSizeStr(size),
                    dvmJdwpStepDepthStr(depth));

                pEvent->mods[idx].step.threadId = threadId;
                pEvent->mods[idx].step.size = size;
                pEvent->mods[idx].step.depth = depth;
            }
            break;
        case MK_INSTANCE_ONLY:  /* report events related to a specific obj */
            {
                ObjectId instance = dvmReadObjectId(&buf);
                LOGVV("    InstanceOnly: %llx", instance);
                pEvent->mods[idx].instanceOnly.objectId = instance;
            }
            break;
        default:
            ALOGW("GLITCH: unsupported modKind=%d", modKind);
            break;
        }
    }

    /*
     * Make sure we consumed all data.  It is possible that the remote side
     * has sent us bad stuff, but for now we blame ourselves.
     */
    if (buf != origBuf + dataLen) {
        ALOGW("GLITCH: dataLen is %d, we have consumed %d", dataLen,
            (int) (buf - origBuf));
    }

    /*
     * We reply with an integer "requestID".
     */
    u4 requestId = dvmJdwpNextEventSerial(state);
    expandBufAdd4BE(pReply, requestId);

    pEvent->requestId = requestId;

    ALOGV("    --> event requestId=%#x", requestId);

    /* add it to the list */
    JdwpError err = dvmJdwpRegisterEvent(state, pEvent);
    if (err != ERR_NONE) {
        /* registration failed, probably because event is bogus */
        dvmJdwpEventFree(pEvent);
        ALOGW("WARNING: event request rejected");
    }
    return err;
}

/*
 * Clear an event.  Failure to find an event with a matching ID is a no-op
 * and does not return an error.
 */
static JdwpError handleER_Clear(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    u1 eventKind;
    eventKind = read1(&buf);
    u4 requestId = read4BE(&buf);

    ALOGV("  Req to clear eventKind=%d requestId=%#x", eventKind, requestId);

    dvmJdwpUnregisterEventById(state, requestId);

    return ERR_NONE;
}

/*
 * Return the values of arguments and local variables.
 */
static JdwpError handleSF_GetValues(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadId = dvmReadObjectId(&buf);
    FrameId frameId = dvmReadFrameId(&buf);
    u4 slots = read4BE(&buf);

    ALOGV("  Req for %d slots in threadId=%llx frameId=%llx",
        slots, threadId, frameId);

    expandBufAdd4BE(pReply, slots);     /* "int values" */
    for (u4 i = 0; i < slots; i++) {
        u4 slot = read4BE(&buf);
        u1 reqSigByte = read1(&buf);

        ALOGV("    --> slot %d '%c'", slot, reqSigByte);

        int width = dvmDbgGetTagWidth(reqSigByte);
        u1* ptr = expandBufAddSpace(pReply, width+1);
        dvmDbgGetLocalValue(threadId, frameId, slot, reqSigByte, ptr, width);
    }

    return ERR_NONE;
}

/*
 * Set the values of arguments and local variables.
 */
static JdwpError handleSF_SetValues(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadId = dvmReadObjectId(&buf);
    FrameId frameId = dvmReadFrameId(&buf);
    u4 slots = read4BE(&buf);

    ALOGV("  Req to set %d slots in threadId=%llx frameId=%llx",
        slots, threadId, frameId);

    for (u4 i = 0; i < slots; i++) {
        u4 slot = read4BE(&buf);
        u1 sigByte = read1(&buf);
        int width = dvmDbgGetTagWidth(sigByte);
        u8 value = jdwpReadValue(&buf, width);

        ALOGV("    --> slot %d '%c' %llx", slot, sigByte, value);
        dvmDbgSetLocalValue(threadId, frameId, slot, sigByte, value, width);
    }

    return ERR_NONE;
}

/*
 * Returns the value of "this" for the specified frame.
 */
static JdwpError handleSF_ThisObject(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    ObjectId threadId = dvmReadObjectId(&buf);
    FrameId frameId = dvmReadFrameId(&buf);

    ObjectId objectId;
    if (!dvmDbgGetThisObject(threadId, frameId, &objectId))
        return ERR_INVALID_FRAMEID;

    u1 objectTag = dvmDbgGetObjectTag(objectId);
    ALOGV("  Req for 'this' in thread=%llx frame=%llx --> %llx %s '%c'",
        threadId, frameId, objectId, dvmDbgGetObjectTypeName(objectId),
        (char)objectTag);

    expandBufAdd1(pReply, objectTag);
    expandBufAddObjectId(pReply, objectId);

    return ERR_NONE;
}

/*
 * Return the reference type reflected by this class object.
 *
 * This appears to be required because ReferenceTypeId values are NEVER
 * reused, whereas ClassIds can be recycled like any other object.  (Either
 * that, or I have no idea what this is for.)
 */
static JdwpError handleCOR_ReflectedType(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    RefTypeId classObjectId = dvmReadRefTypeId(&buf);

    ALOGV("  Req for refTypeId for class=%llx (%s)",
        classObjectId, dvmDbgGetClassDescriptor(classObjectId));

    /* just hand the type back to them */
    if (dvmDbgIsInterface(classObjectId))
        expandBufAdd1(pReply, TT_INTERFACE);
    else
        expandBufAdd1(pReply, TT_CLASS);
    expandBufAddRefTypeId(pReply, classObjectId);

    return ERR_NONE;
}

/*
 * Handle a DDM packet with a single chunk in it.
 */
static JdwpError handleDDM_Chunk(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    u1* replyBuf = NULL;
    int replyLen = -1;

    ALOGV("  Handling DDM packet (%.4s)", buf);

    /*
     * On first DDM packet, notify all handlers that DDM is running.
     */
    if (!state->ddmActive) {
        state->ddmActive = true;
        dvmDbgDdmConnected();
    }

    /*
     * If they want to send something back, we copy it into the buffer.
     * A no-copy approach would be nicer.
     *
     * TODO: consider altering the JDWP stuff to hold the packet header
     * in a separate buffer.  That would allow us to writev() DDM traffic
     * instead of copying it into the expanding buffer.  The reduction in
     * heap requirements is probably more valuable than the efficiency.
     */
    if (dvmDbgDdmHandlePacket(buf, dataLen, &replyBuf, &replyLen)) {
        memcpy(expandBufAddSpace(pReply, replyLen), replyBuf, replyLen);
        free(replyBuf);
    }
    return ERR_NONE;
}

/*
 * Handler map decl.
 */
typedef JdwpError (*JdwpRequestHandler)(JdwpState* state,
    const u1* buf, int dataLen, ExpandBuf* reply);

struct JdwpHandlerMap {
    u1  cmdSet;
    u1  cmd;
    JdwpRequestHandler  func;
    const char* descr;
};

/*
 * Map commands to functions.
 *
 * Command sets 0-63 are incoming requests, 64-127 are outbound requests,
 * and 128-256 are vendor-defined.
 */
static const JdwpHandlerMap gHandlerMap[] = {
    /* VirtualMachine command set (1) */
    { 1,    1,  handleVM_Version,       "VirtualMachine.Version" },
    { 1,    2,  handleVM_ClassesBySignature,
                                        "VirtualMachine.ClassesBySignature" },
    //1,    3,  VirtualMachine.AllClasses
    { 1,    4,  handleVM_AllThreads,    "VirtualMachine.AllThreads" },
    { 1,    5,  handleVM_TopLevelThreadGroups,
                                        "VirtualMachine.TopLevelThreadGroups" },
    { 1,    6,  handleVM_Dispose,       "VirtualMachine.Dispose" },
    { 1,    7,  handleVM_IDSizes,       "VirtualMachine.IDSizes" },
    { 1,    8,  handleVM_Suspend,       "VirtualMachine.Suspend" },
    { 1,    9,  handleVM_Resume,        "VirtualMachine.Resume" },
    { 1,    10, handleVM_Exit,          "VirtualMachine.Exit" },
    { 1,    11, handleVM_CreateString,  "VirtualMachine.CreateString" },
    { 1,    12, handleVM_Capabilities,  "VirtualMachine.Capabilities" },
    { 1,    13, handleVM_ClassPaths,    "VirtualMachine.ClassPaths" },
    { 1,    14, HandleVM_DisposeObjects, "VirtualMachine.DisposeObjects" },
    //1,    15, HoldEvents
    //1,    16, ReleaseEvents
    { 1,    17, handleVM_CapabilitiesNew,
                                        "VirtualMachine.CapabilitiesNew" },
    //1,    18, RedefineClasses
    //1,    19, SetDefaultStratum
    { 1,    20, handleVM_AllClassesWithGeneric,
                                        "VirtualMachine.AllClassesWithGeneric"},
    //1,    21, InstanceCounts

    /* ReferenceType command set (2) */
    { 2,    1,  handleRT_Signature,     "ReferenceType.Signature" },
    { 2,    2,  handleRT_ClassLoader,   "ReferenceType.ClassLoader" },
    { 2,    3,  handleRT_Modifiers,     "ReferenceType.Modifiers" },
    //2,    4,  Fields
    //2,    5,  Methods
    { 2,    6,  handleRT_GetValues,     "ReferenceType.GetValues" },
    { 2,    7,  handleRT_SourceFile,    "ReferenceType.SourceFile" },
    //2,    8,  NestedTypes
    { 2,    9,  handleRT_Status,        "ReferenceType.Status" },
    { 2,    10, handleRT_Interfaces,    "ReferenceType.Interfaces" },
    { 2,    11, handleRT_ClassObject,   "ReferenceType.ClassObject" },
    { 2,    12, handleRT_SourceDebugExtension,
                                        "ReferenceType.SourceDebugExtension" },
    { 2,    13, handleRT_SignatureWithGeneric,
                                        "ReferenceType.SignatureWithGeneric" },
    { 2,    14, handleRT_FieldsWithGeneric,
                                        "ReferenceType.FieldsWithGeneric" },
    { 2,    15, handleRT_MethodsWithGeneric,
                                        "ReferenceType.MethodsWithGeneric" },
    //2,    16, Instances
    //2,    17, ClassFileVersion
    //2,    18, ConstantPool

    /* ClassType command set (3) */
    { 3,    1,  handleCT_Superclass,    "ClassType.Superclass" },
    { 3,    2,  handleCT_SetValues,     "ClassType.SetValues" },
    { 3,    3,  handleCT_InvokeMethod,  "ClassType.InvokeMethod" },
    { 3,    4,  handleCT_NewInstance,   "ClassType.NewInstance" },

    /* ArrayType command set (4) */
    { 4,    1,  handleAT_newInstance,   "ArrayType.NewInstance" },

    /* InterfaceType command set (5) */

    /* Method command set (6) */
    { 6,    1,  handleM_LineTable,      "Method.LineTable" },
    //6,    2,  VariableTable
    //6,    3,  Bytecodes
    //6,    4,  IsObsolete
    { 6,    5,  handleM_VariableTableWithGeneric,
                                        "Method.VariableTableWithGeneric" },

    /* Field command set (8) */

    /* ObjectReference command set (9) */
    { 9,    1,  handleOR_ReferenceType, "ObjectReference.ReferenceType" },
    { 9,    2,  handleOR_GetValues,     "ObjectReference.GetValues" },
    { 9,    3,  handleOR_SetValues,     "ObjectReference.SetValues" },
    //9,    4,  (not defined)
    //9,    5,  MonitorInfo
    { 9,    6,  handleOR_InvokeMethod,  "ObjectReference.InvokeMethod" },
    { 9,    7,  handleOR_DisableCollection,
                                        "ObjectReference.DisableCollection" },
    { 9,    8,  handleOR_EnableCollection,
                                        "ObjectReference.EnableCollection" },
    { 9,    9,  handleOR_IsCollected,   "ObjectReference.IsCollected" },
    //9,    10, ReferringObjects

    /* StringReference command set (10) */
    { 10,   1,  handleSR_Value,         "StringReference.Value" },

    /* ThreadReference command set (11) */
    { 11,   1,  handleTR_Name,          "ThreadReference.Name" },
    { 11,   2,  handleTR_Suspend,       "ThreadReference.Suspend" },
    { 11,   3,  handleTR_Resume,        "ThreadReference.Resume" },
    { 11,   4,  handleTR_Status,        "ThreadReference.Status" },
    { 11,   5,  handleTR_ThreadGroup,   "ThreadReference.ThreadGroup" },
    { 11,   6,  handleTR_Frames,        "ThreadReference.Frames" },
    { 11,   7,  handleTR_FrameCount,    "ThreadReference.FrameCount" },
    //11,   8,  OwnedMonitors
    { 11,   9,  handleTR_CurrentContendedMonitor,
                                    "ThreadReference.CurrentContendedMonitor" },
    //11,   10, Stop
    //11,   11, Interrupt
    { 11,   12, handleTR_SuspendCount,  "ThreadReference.SuspendCount" },
    //11,   13, OwnedMonitorsStackDepthInfo
    //11,   14, ForceEarlyReturn

    /* ThreadGroupReference command set (12) */
    { 12,   1,  handleTGR_Name,         "ThreadGroupReference.Name" },
    { 12,   2,  handleTGR_Parent,       "ThreadGroupReference.Parent" },
    { 12,   3,  handleTGR_Children,     "ThreadGroupReference.Children" },

    /* ArrayReference command set (13) */
    { 13,   1,  handleAR_Length,        "ArrayReference.Length" },
    { 13,   2,  handleAR_GetValues,     "ArrayReference.GetValues" },
    { 13,   3,  handleAR_SetValues,     "ArrayReference.SetValues" },

    /* ClassLoaderReference command set (14) */
    { 14,   1,  handleCLR_VisibleClasses,
                                        "ClassLoaderReference.VisibleClasses" },

    /* EventRequest command set (15) */
    { 15,   1,  handleER_Set,           "EventRequest.Set" },
    { 15,   2,  handleER_Clear,         "EventRequest.Clear" },
    //15,   3,  ClearAllBreakpoints

    /* StackFrame command set (16) */
    { 16,   1,  handleSF_GetValues,     "StackFrame.GetValues" },
    { 16,   2,  handleSF_SetValues,     "StackFrame.SetValues" },
    { 16,   3,  handleSF_ThisObject,    "StackFrame.ThisObject" },
    //16,   4,  PopFrames

    /* ClassObjectReference command set (17) */
    { 17,   1,  handleCOR_ReflectedType,"ClassObjectReference.ReflectedType" },

    /* Event command set (64) */
    //64,  100, Composite   <-- sent from VM to debugger, never received by VM

    { 199,  1,  handleDDM_Chunk,        "DDM.Chunk" },
};


/*
 * Process a request from the debugger.
 *
 * On entry, the JDWP thread is in VMWAIT.
 */
void dvmJdwpProcessRequest(JdwpState* state, const JdwpReqHeader* pHeader,
    const u1* buf, int dataLen, ExpandBuf* pReply)
{
    JdwpError result = ERR_NONE;
    int i, respLen;

    if (pHeader->cmdSet != kJDWPDdmCmdSet) {
        /*
         * Activity from a debugger, not merely ddms.  Mark us as having an
         * active debugger session, and zero out the last-activity timestamp
         * so waitForDebugger() doesn't return if we stall for a bit here.
         */
        dvmDbgActive();
        dvmQuasiAtomicSwap64(0, &state->lastActivityWhen);
    }

    /*
     * If a debugger event has fired in another thread, wait until the
     * initiating thread has suspended itself before processing messages
     * from the debugger.  Otherwise we (the JDWP thread) could be told to
     * resume the thread before it has suspended.
     *
     * We call with an argument of zero to wait for the current event
     * thread to finish, and then clear the block.  Depending on the thread
     * suspend policy, this may allow events in other threads to fire,
     * but those events have no bearing on what the debugger has sent us
     * in the current request.
     *
     * Note that we MUST clear the event token before waking the event
     * thread up, or risk waiting for the thread to suspend after we've
     * told it to resume.
     */
    dvmJdwpSetWaitForEventThread(state, 0);

    /*
     * Tell the VM that we're running and shouldn't be interrupted by GC.
     * Do this after anything that can stall indefinitely.
     */
    dvmDbgThreadRunning();

    expandBufAddSpace(pReply, kJDWPHeaderLen);

    for (i = 0; i < (int) NELEM(gHandlerMap); i++) {
        if (gHandlerMap[i].cmdSet == pHeader->cmdSet &&
            gHandlerMap[i].cmd == pHeader->cmd)
        {
            ALOGV("REQ: %s (cmd=%d/%d dataLen=%d id=0x%06x)",
                gHandlerMap[i].descr, pHeader->cmdSet, pHeader->cmd,
                dataLen, pHeader->id);
            result = (*gHandlerMap[i].func)(state, buf, dataLen, pReply);
            break;
        }
    }
    if (i == NELEM(gHandlerMap)) {
        ALOGE("REQ: UNSUPPORTED (cmd=%d/%d dataLen=%d id=0x%06x)",
            pHeader->cmdSet, pHeader->cmd, dataLen, pHeader->id);
        if (dataLen > 0)
            dvmPrintHexDumpDbg(buf, dataLen, LOG_TAG);
        assert(!"command not implemented");      // make it *really* obvious
        result = ERR_NOT_IMPLEMENTED;
    }

    /*
     * Set up the reply header.
     *
     * If we encountered an error, only send the header back.
     */
    u1* replyBuf = expandBufGetBuffer(pReply);
    set4BE(replyBuf + 4, pHeader->id);
    set1(replyBuf + 8, kJDWPFlagReply);
    set2BE(replyBuf + 9, result);
    if (result == ERR_NONE)
        set4BE(replyBuf + 0, expandBufGetLength(pReply));
    else
        set4BE(replyBuf + 0, kJDWPHeaderLen);

    respLen = expandBufGetLength(pReply) - kJDWPHeaderLen;
    IF_ALOG(LOG_VERBOSE, LOG_TAG) {
        ALOGV("reply: dataLen=%d err=%s(%d)%s", respLen,
            dvmJdwpErrorStr(result), result,
            result != ERR_NONE ? " **FAILED**" : "");
        if (respLen > 0)
            dvmPrintHexDumpDbg(expandBufGetBuffer(pReply) + kJDWPHeaderLen,
                respLen, LOG_TAG);
    }

    /*
     * Update last-activity timestamp.  We really only need this during
     * the initial setup.  Only update if this is a non-DDMS packet.
     */
    if (pHeader->cmdSet != kJDWPDdmCmdSet) {
        dvmQuasiAtomicSwap64(dvmJdwpGetNowMsec(), &state->lastActivityWhen);
    }

    /* tell the VM that GC is okay again */
    dvmDbgThreadWaiting();
}
