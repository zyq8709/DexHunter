/*
 * Copyright (C) 2010 The Android Open Source Project
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
 * dalvik.bytecode.OpcodeInfo
 *
 * This file mostly exists in its current form so that we don't have
 * to have duplicate definitions for things both in libcore and in
 * libdex.
 */

#include "Dalvik.h"
#include "native/InternalNativePriv.h"

/*
 * public static native boolean isInvoke(int opcode);
 */
static void Dalvik_dalvik_bytecode_OpcodeInfo_isInvoke(const u4* args,
    JValue* pResult)
{
    Opcode opcode = static_cast<Opcode>(args[0]);
    int flags = dexGetFlagsFromOpcode(opcode);
    bool result = (flags & kInstrInvoke) != 0;
    RETURN_BOOLEAN(result);
}

const DalvikNativeMethod dvm_dalvik_bytecode_OpcodeInfo[] = {
    { "isInvoke", "(I)Z", Dalvik_dalvik_bytecode_OpcodeInfo_isInvoke },
    { NULL, NULL, NULL },
};
