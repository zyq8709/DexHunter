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

 #define _CODEGEN_C

#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "libdex/DexOpcodes.h"
#include "compiler/CompilerInternals.h"
#include "compiler/codegen/mips/MipsLIR.h"
#include "mterp/common/FindInterface.h"
#include "compiler/codegen/mips/Ralloc.h"
#include "compiler/codegen/mips/Codegen.h"
#include "compiler/Loop.h"
#include "ArchVariant.h"

/* Architectural independent building blocks */
#include "../CodegenCommon.cpp"

/* Architectural independent building blocks */
#include "../Mips32/Factory.cpp"
/* Factory utilities dependent on arch-specific features */
#include "../CodegenFactory.cpp"

/* Thumb-specific codegen routines */
#include "../Mips32/Gen.cpp"
/* Thumb+Portable FP codegen routines */
#include "../FP/MipsFP.cpp"

/* Thumb-specific register allocation */
#include "../Mips32/Ralloc.cpp"

/* MIR2LIR dispatcher and architectural independent codegen routines */
#include "../CodegenDriver.cpp"

/* Dummy driver for method-based JIT */
#include "MethodCodegenDriver.cpp"

/* Architecture manifest */
#include "ArchVariant.cpp"
