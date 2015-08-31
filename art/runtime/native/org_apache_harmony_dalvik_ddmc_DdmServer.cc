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

#include "base/logging.h"
#include "debugger.h"
#include "scoped_thread_state_change.h"
#include "ScopedPrimitiveArray.h"

namespace art {

static void DdmServer_nativeSendChunk(JNIEnv* env, jclass, jint type,
                                      jbyteArray javaData, jint offset, jint length) {
  ScopedObjectAccess soa(env);
  ScopedByteArrayRO data(env, javaData);
  DCHECK_LE(offset + length, static_cast<int32_t>(data.size()));
  Dbg::DdmSendChunk(type, length, reinterpret_cast<const uint8_t*>(&data[offset]));
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(DdmServer, nativeSendChunk, "(I[BII)V"),
};

void register_org_apache_harmony_dalvik_ddmc_DdmServer(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("org/apache/harmony/dalvik/ddmc/DdmServer");
}

}  // namespace art
