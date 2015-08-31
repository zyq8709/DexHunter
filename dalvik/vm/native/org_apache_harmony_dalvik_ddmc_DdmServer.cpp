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
 * org.apache.harmony.dalvik.ddmc.DdmServer
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"


/*
 * private static void nativeSendChunk(int type, byte[] data,
 *      int offset, int length)
 *
 * Send a DDM chunk to the server.
 */
static void Dalvik_org_apache_harmony_dalvik_ddmc_DdmServer_nativeSendChunk(
    const u4* args, JValue* pResult)
{
    int type = args[0];
    ArrayObject* data = (ArrayObject*) args[1];
    int offset = args[2];
    int length = args[3];

    assert(offset+length <= (int)data->length);

    dvmDbgDdmSendChunk(type, length, (const u1*)data->contents + offset);
    RETURN_VOID();
}

const DalvikNativeMethod dvm_org_apache_harmony_dalvik_ddmc_DdmServer[] = {
    { "nativeSendChunk",    "(I[BII)V",
        Dalvik_org_apache_harmony_dalvik_ddmc_DdmServer_nativeSendChunk },
    { NULL, NULL, NULL },
};
