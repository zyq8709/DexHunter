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
 * Some declarations used throughout mterp.
 */
#ifndef DALVIK_MTERP_MTERP_H_
#define DALVIK_MTERP_MTERP_H_

#include "Dalvik.h"
#include "interp/InterpDefs.h"
#if defined(WITH_JIT)
#include "interp/Jit.h"
#endif

/*
 * Call this during initialization to verify that the values in asm-constants.h
 * are still correct.
 */
extern "C" bool dvmCheckAsmConstants(void);

/*
 * Local entry and exit points.  The platform-specific implementation must
 * provide these two.
 */
extern "C" void dvmMterpStdRun(Thread* self);
extern "C" void dvmMterpStdBail(Thread* self);

/*
 * Helper for common_printMethod(), invoked from the assembly
 * interpreter.
 */
extern "C" void dvmMterpPrintMethod(Method* method);

#endif  // DALVIK_MTERP_MTERP_H_
