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

#ifndef ART_COMPILER_LLVM_BACKEND_OPTIONS_H_
#define ART_COMPILER_LLVM_BACKEND_OPTIONS_H_

#include <llvm/Support/CommandLine.h>

#define DECLARE_ARM_BACKEND_OPTIONS \
extern llvm::cl::opt<bool> EnableARMLongCalls; \
extern llvm::cl::opt<bool> ReserveR9;

#define INITIAL_ARM_BACKEND_OPTIONS \
EnableARMLongCalls = true; \
ReserveR9 = true;

#define DECLARE_X86_BACKEND_OPTIONS
#define INITIAL_X86_BACKEND_OPTIONS

#define DECLARE_Mips_BACKEND_OPTIONS
#define INITIAL_Mips_BACKEND_OPTIONS

#define LLVM_TARGET(TargetName) DECLARE_##TargetName##_BACKEND_OPTIONS
#include "llvm/Config/Targets.def"

namespace art {
namespace llvm {

inline void InitialBackendOptions() {
#define LLVM_TARGET(TargetName) INITIAL_##TargetName##_BACKEND_OPTIONS
#include "llvm/Config/Targets.def"
}

}  // namespace llvm
}  // namespace art

#endif  // ART_COMPILER_LLVM_BACKEND_OPTIONS_H_
