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
 * Handle requests.
 */
#ifndef DALVIK_JDWP_JDWPHANDLER_H_
#define DALVIK_JDWP_JDWPHANDLER_H_

#include "Common.h"
#include "ExpandBuf.h"

/*
 * JDWP message header for a request.
 */
struct JdwpReqHeader {
    u4  length;
    u4  id;
    u1  cmdSet;
    u1  cmd;
};

/*
 * Process a request from the debugger.
 *
 * "buf" points past the header, to the content of the message.  "dataLen"
 * can therefore be zero.
 */
void dvmJdwpProcessRequest(JdwpState* state, const JdwpReqHeader* pHeader,
    const u1* buf, int dataLen, ExpandBuf* pReply);

/* helper function */
void dvmJdwpAddLocation(ExpandBuf* pReply, const JdwpLocation* pLoc);

#endif  // DALVIK_JDWP_JDWPHANDLER_H_
