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
 * Dalvik verification subroutines.
 */
#include "Dalvik.h"
#include "analysis/CodeVerify.h"
#include "libdex/InstrUtils.h"


/*
 * This is used when debugging to apply a magnifying glass to the
 * verification of a particular method.
 */
bool dvmWantVerboseVerification(const Method* meth)
{
    return false;       /* COMMENT OUT to enable verbose debugging */

    const char* cd = "Lcom/android/server/am/ActivityManagerService;";
    const char* mn = "trimApplications";
    const char* sg = "()V";
    return (strcmp(meth->clazz->descriptor, cd) == 0 &&
            dvmCompareNameDescriptorAndMethod(mn, sg, meth) == 0);
}

/*
 * Output a code verifier warning message.  For the pre-verifier it's not
 * a big deal if something fails (and it may even be expected), but if
 * we're doing just-in-time verification it's significant.
 */
void dvmLogVerifyFailure(const Method* meth, const char* format, ...)
{
    va_list ap;
    int logLevel;

    if (gDvm.optimizing) {
        return;
        //logLevel = ANDROID_LOG_DEBUG;
    } else {
        logLevel = ANDROID_LOG_WARN;
    }

    va_start(ap, format);
    LOG_PRI_VA(logLevel, LOG_TAG, format, ap);
    if (meth != NULL) {
        char* desc = dexProtoCopyMethodDescriptor(&meth->prototype);
        LOG_PRI(logLevel, LOG_TAG, "VFY:  rejected %s.%s %s",
            meth->clazz->descriptor, meth->name, desc);
        free(desc);
    }
}

/*
 * Show a relatively human-readable message describing the failure to
 * resolve a class.
 *
 * TODO: this is somewhat misleading when resolution fails because of
 * illegal access rather than nonexistent class.
 */
void dvmLogUnableToResolveClass(const char* missingClassDescr,
    const Method* meth)
{
    if (gDvm.optimizing) {
        return;
    }

    std::string dotMissingClass = dvmHumanReadableDescriptor(missingClassDescr);
    std::string dotFromClass = dvmHumanReadableDescriptor(meth->clazz->descriptor);
    ALOGE("Could not find class '%s', referenced from method %s.%s",
            dotMissingClass.c_str(), dotFromClass.c_str(), meth->name);
}

/*
 * Extract the relative offset from a branch instruction.
 *
 * Returns "false" on failure (e.g. this isn't a branch instruction).
 */
bool dvmGetBranchOffset(const Method* meth, const InsnFlags* insnFlags,
    int curOffset, s4* pOffset, bool* pConditional)
{
    const u2* insns = meth->insns + curOffset;

    switch (*insns & 0xff) {
    case OP_GOTO:
        *pOffset = ((s2) *insns) >> 8;
        *pConditional = false;
        break;
    case OP_GOTO_32:
        *pOffset = insns[1] | (((u4) insns[2]) << 16);
        *pConditional = false;
        break;
    case OP_GOTO_16:
        *pOffset = (s2) insns[1];
        *pConditional = false;
        break;
    case OP_IF_EQ:
    case OP_IF_NE:
    case OP_IF_LT:
    case OP_IF_GE:
    case OP_IF_GT:
    case OP_IF_LE:
    case OP_IF_EQZ:
    case OP_IF_NEZ:
    case OP_IF_LTZ:
    case OP_IF_GEZ:
    case OP_IF_GTZ:
    case OP_IF_LEZ:
        *pOffset = (s2) insns[1];
        *pConditional = true;
        break;
    default:
        return false;
        break;
    }

    return true;
}
