/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef DALVIK_VM_COMPILER_OPTIMIZATION_H_
#define DALVIK_VM_COMPILER_OPTIMIZATION_H_

#include "Dalvik.h"

/*
 * If the corresponding bit is set in gDvmJit.disableOpt, the selected
 * optimization will be suppressed.
 */
enum optControlVector {
    kLoadStoreElimination = 0,
    kLoadHoisting,
    kTrackLiveTemps,
    kSuppressLoads,
    kMethodInlining,
    kMethodJit,
};

/* Forward declarations */
struct CompilationUnit;
struct LIR;

void dvmCompilerApplyLocalOptimizations(struct CompilationUnit *cUnit,
                                        struct LIR *head,
                                        struct LIR *tail);

void dvmCompilerApplyGlobalOptimizations(struct CompilationUnit *cUnit);

#endif  // DALVIK_VM_COMPILER_OPTIMIZATION_H_
