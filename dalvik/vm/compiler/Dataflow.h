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

#ifndef DALVIK_VM_DATAFLOW_H_
#define DALVIK_VM_DATAFLOW_H_

#include "Dalvik.h"
#include "CompilerInternals.h"

typedef enum DataFlowAttributePos {
    kUA = 0,
    kUB,
    kUC,
    kUAWide,
    kUBWide,
    kUCWide,
    kDA,
    kDAWide,
    kIsMove,
    kIsLinear,
    kSetsConst,
    kFormat35c,
    kFormat3rc,
    kPhi,
    kNullNRangeCheck0,
    kNullNRangeCheck1,
    kNullNRangeCheck2,
    kFPA,
    kFPB,
    kFPC,
    kGetter,
    kSetter,
} DataFlowAttributes;

#define DF_NOP                  0
#define DF_UA                   (1 << kUA)
#define DF_UB                   (1 << kUB)
#define DF_UC                   (1 << kUC)
#define DF_UA_WIDE              (1 << kUAWide)
#define DF_UB_WIDE              (1 << kUBWide)
#define DF_UC_WIDE              (1 << kUCWide)
#define DF_DA                   (1 << kDA)
#define DF_DA_WIDE              (1 << kDAWide)
#define DF_IS_MOVE              (1 << kIsMove)
#define DF_IS_LINEAR            (1 << kIsLinear)
#define DF_SETS_CONST           (1 << kSetsConst)
#define DF_FORMAT_35C           (1 << kFormat35c)
#define DF_FORMAT_3RC           (1 << kFormat3rc)
#define DF_PHI                  (1 << kPhi)
#define DF_NULL_N_RANGE_CHECK_0 (1 << kNullNRangeCheck0)
#define DF_NULL_N_RANGE_CHECK_1 (1 << kNullNRangeCheck1)
#define DF_NULL_N_RANGE_CHECK_2 (1 << kNullNRangeCheck2)
#define DF_FP_A                 (1 << kFPA)
#define DF_FP_B                 (1 << kFPB)
#define DF_FP_C                 (1 << kFPC)
#define DF_IS_GETTER            (1 << kGetter)
#define DF_IS_SETTER            (1 << kSetter)

#define DF_HAS_USES             (DF_UA | DF_UB | DF_UC | DF_UA_WIDE | \
                                 DF_UB_WIDE | DF_UC_WIDE)

#define DF_HAS_DEFS             (DF_DA | DF_DA_WIDE)

#define DF_HAS_NR_CHECKS        (DF_NULL_N_RANGE_CHECK_0 | \
                                 DF_NULL_N_RANGE_CHECK_1 | \
                                 DF_NULL_N_RANGE_CHECK_2)

#define DF_A_IS_REG             (DF_UA | DF_UA_WIDE | DF_DA | DF_DA_WIDE)
#define DF_B_IS_REG             (DF_UB | DF_UB_WIDE)
#define DF_C_IS_REG             (DF_UC | DF_UC_WIDE)
#define DF_IS_GETTER_OR_SETTER  (DF_IS_GETTER | DF_IS_SETTER)

extern int dvmCompilerDataFlowAttributes[kMirOpLast];

typedef struct BasicBlockDataFlow {
    BitVector *useV;
    BitVector *defV;
    BitVector *liveInV;
    BitVector *phiV;
    int *dalvikToSSAMap;
} BasicBlockDataFlow;

typedef struct SSARepresentation {
    int numUses;
    int *uses;
    bool *fpUse;
    int numDefs;
    int *defs;
    bool *fpDef;
} SSARepresentation;

/*
 * An induction variable is represented by "m*i + c", where i is a basic
 * induction variable.
 */
typedef struct InductionVariableInfo {
    int ssaReg;
    int basicSSAReg;
    int m;      // multiplier
    int c;      // constant
    int inc;    // loop incriment
} InductionVariableInfo;

typedef struct ArrayAccessInfo {
    int arrayReg;
    int ivReg;
    int maxC;                   // For DIV - will affect upper bound checking
    int minC;                   // For DIV - will affect lower bound checking
} ArrayAccessInfo;

#define ENCODE_REG_SUB(r,s)             ((s<<16) | r)
#define DECODE_REG(v)                   (v & 0xffff)
#define DECODE_SUB(v)                   (((unsigned int) v) >> 16)

#endif  // DALVIK_VM_DATAFLOW_H_
