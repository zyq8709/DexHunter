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

#ifndef DALVIK_VM_COMPILERCODEGEN_H_
#define DALVIK_VM_COMPILERCODEGEN_H_

#include "compiler/CompilerIR.h"

/* Maximal number of switch cases to have inline chains */
#define MAX_CHAINED_SWITCH_CASES 64

/* Work unit is architecture dependent */
bool dvmCompilerDoWork(CompilerWorkOrder *work);

/* Lower middle-level IR to low-level IR */
#ifndef ARCH_IA32
void dvmCompilerMIR2LIR(CompilationUnit *cUnit);
#else /* ARCH_IA32 */
void dvmCompilerMIR2LIR(CompilationUnit *cUnit, JitTranslationInfo* info);
#endif

/* Lower middle-level IR to low-level IR for the whole method */
void dvmCompilerMethodMIR2LIR(CompilationUnit *cUnit);

/* Assemble LIR into machine code */
void dvmCompilerAssembleLIR(CompilationUnit *cUnit, JitTranslationInfo *info);

/* Perform translation chain operation. */
extern "C" void* dvmJitChain(void* tgtAddr, u4* branchAddr);

/* Install class objects in the literal pool */
void dvmJitInstallClassObjectPointers(CompilationUnit *cUnit,
                                      char *codeAddress);

/* Patch inline cache content for polymorphic callsites */
bool dvmJitPatchInlineCache(void *cellPtr, void *contentPtr);

/* Implemented in the codegen/<target>/ArchUtility.c */
void dvmCompilerCodegenDump(CompilationUnit *cUnit);

/* Implemented in the codegen/<target>/Assembler.c */
void dvmCompilerPatchInlineCache(void);

/* Implemented in codegen/<target>/Ralloc.c */
void dvmCompilerLocalRegAlloc(CompilationUnit *cUnit);

/* Implemented in codegen/<target>/Thumb<version>Util.c */
void dvmCompilerInitializeRegAlloc(CompilationUnit *cUnit);

/* Implemented in codegen/<target>/<target_variant>/ArchVariant.c */
JitInstructionSetType dvmCompilerInstructionSet(void);

/*
 * Implemented in codegen/<target>/<target_variant>/ArchVariant.c
 * Architecture-specific initializations and checks
 */
bool dvmCompilerArchVariantInit(void);

/* Implemented in codegen/<target>/<target_variant>/ArchVariant.c */
int dvmCompilerTargetOptHint(int key);

/* Implemented in codegen/<target>/<target_variant>/ArchVariant.c */
void dvmCompilerGenMemBarrier(CompilationUnit *cUnit, int barrierKind);

#endif  // DALVIK_VM_COMPILERCODEGEN_H_
