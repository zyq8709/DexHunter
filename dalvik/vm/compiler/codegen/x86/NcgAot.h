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



#ifndef _DALVIK_NCG_AOT
#define _DALVIK_NCG_AOT
int ncgAppGetEIP();
int get_eip_API();
int invokeInterpreter(bool fromApp);
int invokeNcg(bool fromApp);
int jumpToInterpNoChain();
int jumpToInterpPunt();
int jumpToExceptionThrown(int exceptionNum);
void callFuncPtr(int funcPtr, const char* funcName);
int call_helper_API(const char* helperName);
int conditional_jump_global_API(
                                ConditionCode cc, const char* target,
                                bool isShortTerm);
int unconditional_jump_global_API(
                                  const char* target, bool isShortTerm);
int load_imm_global_data_API(const char* dataName,
                             OpndSize size,
                             int reg, bool isPhysical);
int load_global_data_API(const char* dataName,
                         OpndSize size,
                         int reg, bool isPhysical);
int load_sd_global_data_API(const char* dataName,
                            int reg, bool isPhysical);
int load_fp_stack_global_data_API(const char* dataName,
                                  OpndSize size);
#endif

