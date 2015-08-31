/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef DALVIK_VM_COMPILER_CODEGEN_MIPS_CALLOUT_HELPER_H_
#define DALVIK_VM_COMPILER_CODEGEN_MIPS_CALLOUT_HELPER_H_

#include "Dalvik.h"

/*
 * Declare/comment prototypes of all native callout functions invoked by the
 * JIT'ed code here and use the LOAD_FUNC_ADDR macro to load the address into
 * a register. In this way we have a centralized place to find out all native
 * helper functions and we can grep for LOAD_FUNC_ADDR to find out all the
 * callsites.
 */

/* Load a statically compiled function address as a constant */
#define LOAD_FUNC_ADDR(cUnit, reg, addr) loadConstant(cUnit, reg, addr)

/* Conversions */
extern "C" float __floatsisf(int op1);             // OP_INT_TO_FLOAT
extern "C" int __fixsfsi(float op1);               // OP_FLOAT_TO_INT
extern "C" float __truncdfsf2(double op1);         // OP_DOUBLE_TO_FLOAT
extern "C" double __extendsfdf2(float op1);        // OP_FLOAT_TO_DOUBLE
extern "C" double __floatsidf(int op1);            // OP_INT_TO_DOUBLE
extern "C" int __fixdfsi(double op1);              // OP_DOUBLE_TO_INT
extern "C" float __floatdisf(long long op1);       // OP_LONG_TO_FLOAT
extern "C" double __floatdidf(long long op1);      // OP_LONG_TO_DOUBLE
extern "C" long long __fixsfdi(float op1);         // OP_FLOAT_TO_LONG
extern "C" long long __fixdfdi(double op1);        // OP_DOUBLE_TO_LONG

/* Single-precision FP arithmetics */
extern "C" float __addsf3(float a, float b);   // OP_ADD_FLOAT[_2ADDR]
extern "C" float __subsf3(float a, float b);   // OP_SUB_FLOAT[_2ADDR]
extern "C" float __divsf3(float a, float b);   // OP_DIV_FLOAT[_2ADDR]
extern "C" float __mulsf3(float a, float b);   // OP_MUL_FLOAT[_2ADDR]
extern "C" float fmodf(float a, float b);          // OP_REM_FLOAT[_2ADDR]

/* Double-precision FP arithmetics */
extern "C" double __adddf3(double a, double b); // OP_ADD_DOUBLE[_2ADDR]
extern "C" double __subdf3(double a, double b); // OP_SUB_DOUBLE[_2ADDR]
extern "C" double __divdf3(double a, double b); // OP_DIV_DOUBLE[_2ADDR]
extern "C" double __muldf3(double a, double b); // OP_MUL_DOUBLE[_2ADDR]
extern "C" double fmod(double a, double b);         // OP_REM_DOUBLE[_2ADDR]

/* Long long arithmetics - OP_REM_LONG[_2ADDR] & OP_DIV_LONG[_2ADDR] */
extern "C" long long __divdi3(long long op1, long long op2);
extern "C" long long __moddi3(long long op1, long long op2);

/* Originally declared in Sync.h */
bool dvmUnlockObject(struct Thread* self, struct Object* obj); //OP_MONITOR_EXIT

/* Originally declared in oo/TypeCheck.h */
bool dvmCanPutArrayElement(const ClassObject* elemClass,   // OP_APUT_OBJECT
                           const ClassObject* arrayClass);
int dvmInstanceofNonTrivial(const ClassObject* instance,   // OP_CHECK_CAST &&
                            const ClassObject* clazz);     // OP_INSTANCE_OF

/* Originally declared in oo/Array.h */
ArrayObject* dvmAllocArrayByClass(ClassObject* arrayClass, // OP_NEW_ARRAY
                                  size_t length, int allocFlags);

/* Originally declared in interp/InterpDefs.h */
bool dvmInterpHandleFillArrayData(ArrayObject* arrayObject,// OP_FILL_ARRAY_DATA
                                  const u2* arrayData);

/* Originally declared in compiler/codegen/mips/Assemble.c */
const Method *dvmJitToPatchPredictedChain(const Method *method,
                                          Thread *self,
                                          PredictedChainingCell *cell,
                                          const ClassObject *clazz);

/*
 * Resolve interface callsites - OP_INVOKE_INTERFACE & OP_INVOKE_INTERFACE_RANGE
 *
 * Originally declared in mterp/common/FindInterface.h and only comment it here
 * due to the INLINE attribute.
 *
 * INLINE Method* dvmFindInterfaceMethodInCache(ClassObject* thisClass,
 *  u4 methodIdx, const Method* method, DvmDex* methodClassDex)
 */

/* Originally declared in alloc/Alloc.h */
Object* dvmAllocObject(ClassObject* clazz, int flags);  // OP_NEW_INSTANCE

/*
 * Functions declared in gDvmInlineOpsTable[] are used for
 * OP_EXECUTE_INLINE & OP_EXECUTE_INLINE_RANGE.
 */
extern "C" double sqrt(double x);  // INLINE_MATH_SQRT

/*
 * The following functions are invoked through the compiler templates (declared
 * in compiler/template/armv5te/footer.S:
 *
 *      __aeabi_cdcmple         // CMPG_DOUBLE
 *      __aeabi_cfcmple         // CMPG_FLOAT
 *      dvmLockObject           // MONITOR_ENTER
 */

#endif  // DALVIK_VM_COMPILER_CODEGEN_MIPS_CALLOUT_HELPER_H_
