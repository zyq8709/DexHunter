/*
 * Copyright (C) 2012 The Android Open Source Project
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


#ifndef _DALVIK_NCG_HELPER
#define _DALVIK_NCG_HELPER
#include "mterp/Mterp.h"
s4 dvmNcgHandlePackedSwitch(const s4*, s4, u2, s4);
s4 dvmNcgHandleSparseSwitch(const s4*, u2, s4);
s4 dvmJitHandlePackedSwitch(const s4*, s4, u2, s4);
s4 dvmJitHandleSparseSwitch(const s4*, u2, s4);
extern "C" void dvmNcgInvokeInterpreter(int pc); //interpreter to execute at pc
extern "C" void dvmNcgInvokeNcg(int pc);
extern "C" void dvmJitToInterpNormal(int targetpc); //in %ebx
extern "C" void dvmJitToInterpTraceSelect(int targetpc); //in %ebx
extern "C" void dvmJitToInterpTraceSelectNoChain(int targetpc); //in %ebx
extern "C" void dvmJitToInterpNoChain(int targetpc); //in %eax
extern "C" void dvmJitToInterpNoChainNoProfile(int targetpc); //in %eax
extern "C" void dvmJitToInterpPunt(int targetpc); //in currentPc
extern "C" void dvmJitToExceptionThrown(int targetpc); //in currentPc
#ifdef DEBUG_CALL_STACK3
void debug_dumpSwitch(int); //in %ebx
#endif

const Method *dvmJitToPatchPredictedChain(const Method *method,
                                          Thread *self,
                                          PredictedChainingCell *cell,
                                          const ClassObject *clazz);
#endif /*_DALVIK_NCG_HELPER*/
