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
 * String constants to go along with enumerated values.  (Pity we don't
 * have enumerated constant reflection in C.)  These are only needed for
 * making the output human-readable.
 */
#include "jdwp/JdwpConstants.h"

/*
 * Return a string for the error code.
 */
const char* dvmJdwpErrorStr(JdwpError error)
{
    switch (error) {
    case ERR_NONE:
        return "NONE";
    case ERR_INVALID_THREAD:
        return "INVALID_THREAD";
    case ERR_INVALID_THREAD_GROUP:
        return "INVALID_THREAD_GROUP";
    case ERR_INVALID_PRIORITY:
        return "INVALID_PRIORITY";
    case ERR_THREAD_NOT_SUSPENDED:
        return "THREAD_NOT_SUSPENDED";
    case ERR_THREAD_SUSPENDED:
        return "THREAD_SUSPENDED";
    case ERR_INVALID_OBJECT:
        return "INVALID_OBJEC";
    case ERR_INVALID_CLASS:
        return "INVALID_CLASS";
    case ERR_CLASS_NOT_PREPARED:
        return "CLASS_NOT_PREPARED";
    case ERR_INVALID_METHODID:
        return "INVALID_METHODID";
    case ERR_INVALID_LOCATION:
        return "INVALID_LOCATION";
    case ERR_INVALID_FIELDID:
        return "INVALID_FIELDID";
    case ERR_INVALID_FRAMEID:
        return "INVALID_FRAMEID";
    case ERR_NO_MORE_FRAMES:
        return "NO_MORE_FRAMES";
    case ERR_OPAQUE_FRAME:
        return "OPAQUE_FRAME";
    case ERR_NOT_CURRENT_FRAME:
        return "NOT_CURRENT_FRAME";
    case ERR_TYPE_MISMATCH:
        return "TYPE_MISMATCH";
    case ERR_INVALID_SLOT:
        return "INVALID_SLOT";
    case ERR_DUPLICATE:
        return "DUPLICATE";
    case ERR_NOT_FOUND:
        return "NOT_FOUND";
    case ERR_INVALID_MONITOR:
        return "INVALID_MONITOR";
    case ERR_NOT_MONITOR_OWNER:
        return "NOT_MONITOR_OWNER";
    case ERR_INTERRUPT:
        return "INTERRUPT";
    case ERR_INVALID_CLASS_FORMAT:
        return "INVALID_CLASS_FORMAT";
    case ERR_CIRCULAR_CLASS_DEFINITION:
        return "CIRCULAR_CLASS_DEFINITION";
    case ERR_FAILS_VERIFICATION:
        return "FAILS_VERIFICATION";
    case ERR_ADD_METHOD_NOT_IMPLEMENTED:
        return "ADD_METHOD_NOT_IMPLEMENTED";
    case ERR_SCHEMA_CHANGE_NOT_IMPLEMENTED:
        return "SCHEMA_CHANGE_NOT_IMPLEMENTED";
    case ERR_INVALID_TYPESTATE:
        return "INVALID_TYPESTATE";
    case ERR_HIERARCHY_CHANGE_NOT_IMPLEMENTED:
        return "HIERARCHY_CHANGE_NOT_IMPLEMENTED";
    case ERR_DELETE_METHOD_NOT_IMPLEMENTED:
        return "DELETE_METHOD_NOT_IMPLEMENTED";
    case ERR_UNSUPPORTED_VERSION:
        return "UNSUPPORTED_VERSION";
    case ERR_NAMES_DONT_MATCH:
        return "NAMES_DONT_MATCH";
    case ERR_CLASS_MODIFIERS_CHANGE_NOT_IMPLEMENTED:
        return "CLASS_MODIFIERS_CHANGE_NOT_IMPLEMENTED";
    case ERR_METHOD_MODIFIERS_CHANGE_NOT_IMPLEMENTED:
        return "METHOD_MODIFIERS_CHANGE_NOT_IMPLEMENTED";
    case ERR_NOT_IMPLEMENTED:
        return "NOT_IMPLEMENTED";
    case ERR_NULL_POINTER:
        return "NULL_POINTER";
    case ERR_ABSENT_INFORMATION:
        return "ABSENT_INFORMATION";
    case ERR_INVALID_EVENT_TYPE:
        return "INVALID_EVENT_TYPE";
    case ERR_ILLEGAL_ARGUMENT:
        return "ILLEGAL_ARGUMENT";
    case ERR_OUT_OF_MEMORY:
        return "OUT_OF_MEMORY";
    case ERR_ACCESS_DENIED:
        return "ACCESS_DENIED";
    case ERR_VM_DEAD:
        return "VM_DEAD";
    case ERR_INTERNAL:
        return "INTERNAL";
    case ERR_UNATTACHED_THREAD:
        return "UNATTACHED_THREAD";
    case ERR_INVALID_TAG:
        return "INVALID_TAG";
    case ERR_ALREADY_INVOKING:
        return "ALREADY_INVOKING";
    case ERR_INVALID_INDEX:
        return "INVALID_INDEX";
    case ERR_INVALID_LENGTH:
        return "INVALID_LENGTH";
    case ERR_INVALID_STRING:
        return "INVALID_STRING";
    case ERR_INVALID_CLASS_LOADER:
        return "INVALID_CLASS_LOADER";
    case ERR_INVALID_ARRAY:
        return "INVALID_ARRAY";
    case ERR_TRANSPORT_LOAD:
        return "TRANSPORT_LOAD";
    case ERR_TRANSPORT_INIT:
        return "TRANSPORT_INIT";
    case ERR_NATIVE_METHOD:
        return "NATIVE_METHOD";
    case ERR_INVALID_COUNT:
        return "INVALID_COUNT";
    default:
        return "?UNKNOWN?";
    }
}

/*
 * Return a string for the EventKind.
 */
const char* dvmJdwpEventKindStr(JdwpEventKind kind)
{
    switch (kind) {
    case EK_SINGLE_STEP:        return "SINGLE_STEP";
    case EK_BREAKPOINT:         return "BREAKPOINT";
    case EK_FRAME_POP:          return "FRAME_POP";
    case EK_EXCEPTION:          return "EXCEPTION";
    case EK_USER_DEFINED:       return "USER_DEFINED";
    case EK_THREAD_START:       return "THREAD_START";
    /*case EK_THREAD_END:         return "THREAD_END";*/
    case EK_CLASS_PREPARE:      return "CLASS_PREPARE";
    case EK_CLASS_UNLOAD:       return "CLASS_UNLOAD";
    case EK_CLASS_LOAD:         return "CLASS_LOAD";
    case EK_FIELD_ACCESS:       return "FIELD_ACCESS";
    case EK_FIELD_MODIFICATION: return "FIELD_MODIFICATION";
    case EK_EXCEPTION_CATCH:    return "EXCEPTION_CATCH";
    case EK_METHOD_ENTRY:       return "METHOD_ENTRY";
    case EK_METHOD_EXIT:        return "METHOD_EXIT";
    case EK_VM_INIT:            return "VM_INIT";
    case EK_VM_DEATH:           return "VM_DEATH";
    case EK_VM_DISCONNECTED:    return "VM_DISCONNECTED";
    /*case EK_VM_START:           return "VM_START";*/
    case EK_THREAD_DEATH:       return "THREAD_DEATH";
    default:                    return "?UNKNOWN?";
    }
}

/*
 * Return a string for the ModKind.
 */
const char* dvmJdwpModKindStr(JdwpModKind kind)
{
    switch (kind) {
    case MK_COUNT:              return "COUNT";
    case MK_CONDITIONAL:        return "CONDITIONAL";
    case MK_THREAD_ONLY:        return "THREAD_ONLY";
    case MK_CLASS_ONLY:         return "CLASS_ONLY";
    case MK_CLASS_MATCH:        return "CLASS_MATCH";
    case MK_CLASS_EXCLUDE:      return "CLASS_EXCLUDE";
    case MK_LOCATION_ONLY:      return "LOCATION_ONLY";
    case MK_EXCEPTION_ONLY:     return "EXCEPTION_ONLY";
    case MK_FIELD_ONLY:         return "FIELD_ONLY";
    case MK_STEP:               return "STEP";
    case MK_INSTANCE_ONLY:      return "INSTANCE_ONLY";
    default:                    return "?UNKNOWN?";
    }
}

/*
 * Return a string for the StepDepth.
 */
const char* dvmJdwpStepDepthStr(JdwpStepDepth depth)
{
    switch (depth) {
    case SD_INTO:               return "INTO";
    case SD_OVER:               return "OVER";
    case SD_OUT:                return "OUT";
    default:                    return "?UNKNOWN?";
    }
}

/*
 * Return a string for the StepSize.
 */
const char* dvmJdwpStepSizeStr(JdwpStepSize size)
{
    switch (size) {
    case SS_MIN:                return "MIN";
    case SS_LINE:               return "LINE";
    default:                    return "?UNKNOWN?";
    }
}

/*
 * Return a string for the SuspendPolicy.
 */
const char* dvmJdwpSuspendPolicyStr(JdwpSuspendPolicy policy)
{
    switch (policy) {
    case SP_NONE:               return "NONE";
    case SP_EVENT_THREAD:       return "EVENT_THREAD";
    case SP_ALL:                return "ALL";
    default:                    return "?UNKNOWN?";
    }
}

/*
 * Return a string for the SuspendStatus.
 */
const char* dvmJdwpSuspendStatusStr(JdwpSuspendStatus status)
{
    switch (status) {
    case SUSPEND_STATUS_NOT_SUSPENDED: return "Not SUSPENDED";
    case SUSPEND_STATUS_SUSPENDED:     return "SUSPENDED";
    default:                           return "?UNKNOWN?";
    }
}

/*
 * Return a string for the ThreadStatus.
 */
const char* dvmJdwpThreadStatusStr(JdwpThreadStatus status)
{
    switch (status) {
    case TS_ZOMBIE:             return "ZOMBIE";
    case TS_RUNNING:            return "RUNNING";
    case TS_SLEEPING:           return "SLEEPING";
    case TS_MONITOR:            return "MONITOR";
    case TS_WAIT:               return "WAIT";
    default:                    return "?UNKNOWN?";
    }
};
