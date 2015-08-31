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
#define _ARMV7_A_NEON
#define TGT_LIR ArmLIR

#include "Dalvik.h"
#include "interp/InterpDefs.h"
#include "libdex/DexOpcodes.h"
#include "compiler/CompilerInternals.h"
#include "compiler/codegen/arm/ArmLIR.h"
#include "mterp/common/FindInterface.h"
#include "compiler/codegen/Ralloc.h"
#include "compiler/codegen/arm/Codegen.h"
#include "compiler/Loop.h"
#include "ArchVariant.h"

/* Arm codegen building blocks */
#include "../CodegenCommon.cpp"

/* Thumb2-specific factory utilities */
#include "../Thumb2/Factory.cpp"
/* Target indepedent factory utilities */
#include "../../CodegenFactory.cpp"
/* Arm-specific factory utilities */
#include "../ArchFactory.cpp"

/* Thumb2-specific codegen routines */
#include "../Thumb2/Gen.cpp"
/* Thumb2+VFP codegen routines */
#include "../FP/Thumb2VFP.cpp"

/* Thumb2-specific register allocation */
#include "../Thumb2/Ralloc.cpp"

/* MIR2LIR dispatcher and architectural independent codegen routines */
#include "../CodegenDriver.cpp"

/* Driver for method-based JIT */
#include "MethodCodegenDriver.cpp"

/* Architecture manifest */
#include "ArchVariant.cpp"
