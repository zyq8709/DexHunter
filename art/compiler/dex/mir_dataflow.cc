/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "compiler_internals.h"
#include "local_value_numbering.h"
#include "dataflow_iterator-inl.h"

namespace art {

/*
 * Main table containing data flow attributes for each bytecode. The
 * first kNumPackedOpcodes entries are for Dalvik bytecode
 * instructions, where extended opcode at the MIR level are appended
 * afterwards.
 *
 * TODO - many optimization flags are incomplete - they will only limit the
 * scope of optimizations but will not cause mis-optimizations.
 */
const int MIRGraph::oat_data_flow_attributes_[kMirOpLast] = {
  // 00 NOP
  DF_NOP,

  // 01 MOVE vA, vB
  DF_DA | DF_UB | DF_IS_MOVE,

  // 02 MOVE_FROM16 vAA, vBBBB
  DF_DA | DF_UB | DF_IS_MOVE,

  // 03 MOVE_16 vAAAA, vBBBB
  DF_DA | DF_UB | DF_IS_MOVE,

  // 04 MOVE_WIDE vA, vB
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_IS_MOVE,

  // 05 MOVE_WIDE_FROM16 vAA, vBBBB
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_IS_MOVE,

  // 06 MOVE_WIDE_16 vAAAA, vBBBB
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_IS_MOVE,

  // 07 MOVE_OBJECT vA, vB
  DF_DA | DF_UB | DF_NULL_TRANSFER_0 | DF_IS_MOVE | DF_REF_A | DF_REF_B,

  // 08 MOVE_OBJECT_FROM16 vAA, vBBBB
  DF_DA | DF_UB | DF_NULL_TRANSFER_0 | DF_IS_MOVE | DF_REF_A | DF_REF_B,

  // 09 MOVE_OBJECT_16 vAAAA, vBBBB
  DF_DA | DF_UB | DF_NULL_TRANSFER_0 | DF_IS_MOVE | DF_REF_A | DF_REF_B,

  // 0A MOVE_RESULT vAA
  DF_DA,

  // 0B MOVE_RESULT_WIDE vAA
  DF_DA | DF_A_WIDE,

  // 0C MOVE_RESULT_OBJECT vAA
  DF_DA | DF_REF_A,

  // 0D MOVE_EXCEPTION vAA
  DF_DA | DF_REF_A | DF_NON_NULL_DST,

  // 0E RETURN_VOID
  DF_NOP,

  // 0F RETURN vAA
  DF_UA,

  // 10 RETURN_WIDE vAA
  DF_UA | DF_A_WIDE,

  // 11 RETURN_OBJECT vAA
  DF_UA | DF_REF_A,

  // 12 CONST_4 vA, #+B
  DF_DA | DF_SETS_CONST,

  // 13 CONST_16 vAA, #+BBBB
  DF_DA | DF_SETS_CONST,

  // 14 CONST vAA, #+BBBBBBBB
  DF_DA | DF_SETS_CONST,

  // 15 CONST_HIGH16 VAA, #+BBBB0000
  DF_DA | DF_SETS_CONST,

  // 16 CONST_WIDE_16 vAA, #+BBBB
  DF_DA | DF_A_WIDE | DF_SETS_CONST,

  // 17 CONST_WIDE_32 vAA, #+BBBBBBBB
  DF_DA | DF_A_WIDE | DF_SETS_CONST,

  // 18 CONST_WIDE vAA, #+BBBBBBBBBBBBBBBB
  DF_DA | DF_A_WIDE | DF_SETS_CONST,

  // 19 CONST_WIDE_HIGH16 vAA, #+BBBB000000000000
  DF_DA | DF_A_WIDE | DF_SETS_CONST,

  // 1A CONST_STRING vAA, string@BBBB
  DF_DA | DF_REF_A | DF_NON_NULL_DST,

  // 1B CONST_STRING_JUMBO vAA, string@BBBBBBBB
  DF_DA | DF_REF_A | DF_NON_NULL_DST,

  // 1C CONST_CLASS vAA, type@BBBB
  DF_DA | DF_REF_A | DF_NON_NULL_DST,

  // 1D MONITOR_ENTER vAA
  DF_UA | DF_NULL_CHK_0 | DF_REF_A,

  // 1E MONITOR_EXIT vAA
  DF_UA | DF_NULL_CHK_0 | DF_REF_A,

  // 1F CHK_CAST vAA, type@BBBB
  DF_UA | DF_REF_A | DF_UMS,

  // 20 INSTANCE_OF vA, vB, type@CCCC
  DF_DA | DF_UB | DF_CORE_A | DF_REF_B | DF_UMS,

  // 21 ARRAY_LENGTH vA, vB
  DF_DA | DF_UB | DF_NULL_CHK_0 | DF_CORE_A | DF_REF_B,

  // 22 NEW_INSTANCE vAA, type@BBBB
  DF_DA | DF_NON_NULL_DST | DF_REF_A | DF_UMS,

  // 23 NEW_ARRAY vA, vB, type@CCCC
  DF_DA | DF_UB | DF_NON_NULL_DST | DF_REF_A | DF_CORE_B | DF_UMS,

  // 24 FILLED_NEW_ARRAY {vD, vE, vF, vG, vA}
  DF_FORMAT_35C | DF_NON_NULL_RET | DF_UMS,

  // 25 FILLED_NEW_ARRAY_RANGE {vCCCC .. vNNNN}, type@BBBB
  DF_FORMAT_3RC | DF_NON_NULL_RET | DF_UMS,

  // 26 FILL_ARRAY_DATA vAA, +BBBBBBBB
  DF_UA | DF_REF_A | DF_UMS,

  // 27 THROW vAA
  DF_UA | DF_REF_A | DF_UMS,

  // 28 GOTO
  DF_NOP,

  // 29 GOTO_16
  DF_NOP,

  // 2A GOTO_32
  DF_NOP,

  // 2B PACKED_SWITCH vAA, +BBBBBBBB
  DF_UA,

  // 2C SPARSE_SWITCH vAA, +BBBBBBBB
  DF_UA,

  // 2D CMPL_FLOAT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_FP_B | DF_FP_C | DF_CORE_A,

  // 2E CMPG_FLOAT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_FP_B | DF_FP_C | DF_CORE_A,

  // 2F CMPL_DOUBLE vAA, vBB, vCC
  DF_DA | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_FP_B | DF_FP_C | DF_CORE_A,

  // 30 CMPG_DOUBLE vAA, vBB, vCC
  DF_DA | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_FP_B | DF_FP_C | DF_CORE_A,

  // 31 CMP_LONG vAA, vBB, vCC
  DF_DA | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 32 IF_EQ vA, vB, +CCCC
  DF_UA | DF_UB,

  // 33 IF_NE vA, vB, +CCCC
  DF_UA | DF_UB,

  // 34 IF_LT vA, vB, +CCCC
  DF_UA | DF_UB,

  // 35 IF_GE vA, vB, +CCCC
  DF_UA | DF_UB,

  // 36 IF_GT vA, vB, +CCCC
  DF_UA | DF_UB,

  // 37 IF_LE vA, vB, +CCCC
  DF_UA | DF_UB,

  // 38 IF_EQZ vAA, +BBBB
  DF_UA,

  // 39 IF_NEZ vAA, +BBBB
  DF_UA,

  // 3A IF_LTZ vAA, +BBBB
  DF_UA,

  // 3B IF_GEZ vAA, +BBBB
  DF_UA,

  // 3C IF_GTZ vAA, +BBBB
  DF_UA,

  // 3D IF_LEZ vAA, +BBBB
  DF_UA,

  // 3E UNUSED_3E
  DF_NOP,

  // 3F UNUSED_3F
  DF_NOP,

  // 40 UNUSED_40
  DF_NOP,

  // 41 UNUSED_41
  DF_NOP,

  // 42 UNUSED_42
  DF_NOP,

  // 43 UNUSED_43
  DF_NOP,

  // 44 AGET vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_NULL_CHK_0 | DF_RANGE_CHK_1 | DF_REF_B | DF_CORE_C,

  // 45 AGET_WIDE vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_UC | DF_NULL_CHK_0 | DF_RANGE_CHK_1 | DF_REF_B | DF_CORE_C,

  // 46 AGET_OBJECT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_NULL_CHK_0 | DF_RANGE_CHK_1 | DF_REF_A | DF_REF_B | DF_CORE_C,

  // 47 AGET_BOOLEAN vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_NULL_CHK_0 | DF_RANGE_CHK_1 | DF_REF_B | DF_CORE_C,

  // 48 AGET_BYTE vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_NULL_CHK_0 | DF_RANGE_CHK_1 | DF_REF_B | DF_CORE_C,

  // 49 AGET_CHAR vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_NULL_CHK_0 | DF_RANGE_CHK_1 | DF_REF_B | DF_CORE_C,

  // 4A AGET_SHORT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_NULL_CHK_0 | DF_RANGE_CHK_1 | DF_REF_B | DF_CORE_C,

  // 4B APUT vAA, vBB, vCC
  DF_UA | DF_UB | DF_UC | DF_NULL_CHK_1 | DF_RANGE_CHK_2 | DF_REF_B | DF_CORE_C,

  // 4C APUT_WIDE vAA, vBB, vCC
  DF_UA | DF_A_WIDE | DF_UB | DF_UC | DF_NULL_CHK_2 | DF_RANGE_CHK_3 | DF_REF_B | DF_CORE_C,

  // 4D APUT_OBJECT vAA, vBB, vCC
  DF_UA | DF_UB | DF_UC | DF_NULL_CHK_1 | DF_RANGE_CHK_2 | DF_REF_A | DF_REF_B | DF_CORE_C,

  // 4E APUT_BOOLEAN vAA, vBB, vCC
  DF_UA | DF_UB | DF_UC | DF_NULL_CHK_1 | DF_RANGE_CHK_2 | DF_REF_B | DF_CORE_C,

  // 4F APUT_BYTE vAA, vBB, vCC
  DF_UA | DF_UB | DF_UC | DF_NULL_CHK_1 | DF_RANGE_CHK_2 | DF_REF_B | DF_CORE_C,

  // 50 APUT_CHAR vAA, vBB, vCC
  DF_UA | DF_UB | DF_UC | DF_NULL_CHK_1 | DF_RANGE_CHK_2 | DF_REF_B | DF_CORE_C,

  // 51 APUT_SHORT vAA, vBB, vCC
  DF_UA | DF_UB | DF_UC | DF_NULL_CHK_1 | DF_RANGE_CHK_2 | DF_REF_B | DF_CORE_C,

  // 52 IGET vA, vB, field@CCCC
  DF_DA | DF_UB | DF_NULL_CHK_0 | DF_REF_B,

  // 53 IGET_WIDE vA, vB, field@CCCC
  DF_DA | DF_A_WIDE | DF_UB | DF_NULL_CHK_0 | DF_REF_B,

  // 54 IGET_OBJECT vA, vB, field@CCCC
  DF_DA | DF_UB | DF_NULL_CHK_0 | DF_REF_A | DF_REF_B,

  // 55 IGET_BOOLEAN vA, vB, field@CCCC
  DF_DA | DF_UB | DF_NULL_CHK_0 | DF_REF_B,

  // 56 IGET_BYTE vA, vB, field@CCCC
  DF_DA | DF_UB | DF_NULL_CHK_0 | DF_REF_B,

  // 57 IGET_CHAR vA, vB, field@CCCC
  DF_DA | DF_UB | DF_NULL_CHK_0 | DF_REF_B,

  // 58 IGET_SHORT vA, vB, field@CCCC
  DF_DA | DF_UB | DF_NULL_CHK_0 | DF_REF_B,

  // 59 IPUT vA, vB, field@CCCC
  DF_UA | DF_UB | DF_NULL_CHK_1 | DF_REF_B,

  // 5A IPUT_WIDE vA, vB, field@CCCC
  DF_UA | DF_A_WIDE | DF_UB | DF_NULL_CHK_2 | DF_REF_B,

  // 5B IPUT_OBJECT vA, vB, field@CCCC
  DF_UA | DF_UB | DF_NULL_CHK_1 | DF_REF_A | DF_REF_B,

  // 5C IPUT_BOOLEAN vA, vB, field@CCCC
  DF_UA | DF_UB | DF_NULL_CHK_1 | DF_REF_B,

  // 5D IPUT_BYTE vA, vB, field@CCCC
  DF_UA | DF_UB | DF_NULL_CHK_1 | DF_REF_B,

  // 5E IPUT_CHAR vA, vB, field@CCCC
  DF_UA | DF_UB | DF_NULL_CHK_1 | DF_REF_B,

  // 5F IPUT_SHORT vA, vB, field@CCCC
  DF_UA | DF_UB | DF_NULL_CHK_1 | DF_REF_B,

  // 60 SGET vAA, field@BBBB
  DF_DA | DF_UMS,

  // 61 SGET_WIDE vAA, field@BBBB
  DF_DA | DF_A_WIDE | DF_UMS,

  // 62 SGET_OBJECT vAA, field@BBBB
  DF_DA | DF_REF_A | DF_UMS,

  // 63 SGET_BOOLEAN vAA, field@BBBB
  DF_DA | DF_UMS,

  // 64 SGET_BYTE vAA, field@BBBB
  DF_DA | DF_UMS,

  // 65 SGET_CHAR vAA, field@BBBB
  DF_DA | DF_UMS,

  // 66 SGET_SHORT vAA, field@BBBB
  DF_DA | DF_UMS,

  // 67 SPUT vAA, field@BBBB
  DF_UA | DF_UMS,

  // 68 SPUT_WIDE vAA, field@BBBB
  DF_UA | DF_A_WIDE | DF_UMS,

  // 69 SPUT_OBJECT vAA, field@BBBB
  DF_UA | DF_REF_A | DF_UMS,

  // 6A SPUT_BOOLEAN vAA, field@BBBB
  DF_UA | DF_UMS,

  // 6B SPUT_BYTE vAA, field@BBBB
  DF_UA | DF_UMS,

  // 6C SPUT_CHAR vAA, field@BBBB
  DF_UA | DF_UMS,

  // 6D SPUT_SHORT vAA, field@BBBB
  DF_UA | DF_UMS,

  // 6E INVOKE_VIRTUAL {vD, vE, vF, vG, vA}
  DF_FORMAT_35C | DF_NULL_CHK_OUT0 | DF_UMS,

  // 6F INVOKE_SUPER {vD, vE, vF, vG, vA}
  DF_FORMAT_35C | DF_NULL_CHK_OUT0 | DF_UMS,

  // 70 INVOKE_DIRECT {vD, vE, vF, vG, vA}
  DF_FORMAT_35C | DF_NULL_CHK_OUT0 | DF_UMS,

  // 71 INVOKE_STATIC {vD, vE, vF, vG, vA}
  DF_FORMAT_35C | DF_UMS,

  // 72 INVOKE_INTERFACE {vD, vE, vF, vG, vA}
  DF_FORMAT_35C | DF_NULL_CHK_OUT0 | DF_UMS,

  // 73 UNUSED_73
  DF_NOP,

  // 74 INVOKE_VIRTUAL_RANGE {vCCCC .. vNNNN}
  DF_FORMAT_3RC | DF_NULL_CHK_OUT0 | DF_UMS,

  // 75 INVOKE_SUPER_RANGE {vCCCC .. vNNNN}
  DF_FORMAT_3RC | DF_NULL_CHK_OUT0 | DF_UMS,

  // 76 INVOKE_DIRECT_RANGE {vCCCC .. vNNNN}
  DF_FORMAT_3RC | DF_NULL_CHK_OUT0 | DF_UMS,

  // 77 INVOKE_STATIC_RANGE {vCCCC .. vNNNN}
  DF_FORMAT_3RC | DF_UMS,

  // 78 INVOKE_INTERFACE_RANGE {vCCCC .. vNNNN}
  DF_FORMAT_3RC | DF_NULL_CHK_OUT0 | DF_UMS,

  // 79 UNUSED_79
  DF_NOP,

  // 7A UNUSED_7A
  DF_NOP,

  // 7B NEG_INT vA, vB
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // 7C NOT_INT vA, vB
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // 7D NEG_LONG vA, vB
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_CORE_A | DF_CORE_B,

  // 7E NOT_LONG vA, vB
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_CORE_A | DF_CORE_B,

  // 7F NEG_FLOAT vA, vB
  DF_DA | DF_UB | DF_FP_A | DF_FP_B,

  // 80 NEG_DOUBLE vA, vB
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_FP_A | DF_FP_B,

  // 81 INT_TO_LONG vA, vB
  DF_DA | DF_A_WIDE | DF_UB | DF_CORE_A | DF_CORE_B,

  // 82 INT_TO_FLOAT vA, vB
  DF_DA | DF_UB | DF_FP_A | DF_CORE_B,

  // 83 INT_TO_DOUBLE vA, vB
  DF_DA | DF_A_WIDE | DF_UB | DF_FP_A | DF_CORE_B,

  // 84 LONG_TO_INT vA, vB
  DF_DA | DF_UB | DF_B_WIDE | DF_CORE_A | DF_CORE_B,

  // 85 LONG_TO_FLOAT vA, vB
  DF_DA | DF_UB | DF_B_WIDE | DF_FP_A | DF_CORE_B,

  // 86 LONG_TO_DOUBLE vA, vB
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_FP_A | DF_CORE_B,

  // 87 FLOAT_TO_INT vA, vB
  DF_DA | DF_UB | DF_FP_B | DF_CORE_A,

  // 88 FLOAT_TO_LONG vA, vB
  DF_DA | DF_A_WIDE | DF_UB | DF_FP_B | DF_CORE_A,

  // 89 FLOAT_TO_DOUBLE vA, vB
  DF_DA | DF_A_WIDE | DF_UB | DF_FP_A | DF_FP_B,

  // 8A DOUBLE_TO_INT vA, vB
  DF_DA | DF_UB | DF_B_WIDE | DF_FP_B | DF_CORE_A,

  // 8B DOUBLE_TO_LONG vA, vB
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_FP_B | DF_CORE_A,

  // 8C DOUBLE_TO_FLOAT vA, vB
  DF_DA | DF_UB | DF_B_WIDE | DF_FP_A | DF_FP_B,

  // 8D INT_TO_BYTE vA, vB
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // 8E INT_TO_CHAR vA, vB
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // 8F INT_TO_SHORT vA, vB
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // 90 ADD_INT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 91 SUB_INT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 92 MUL_INT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 93 DIV_INT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 94 REM_INT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 95 AND_INT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 96 OR_INT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 97 XOR_INT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 98 SHL_INT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 99 SHR_INT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 9A USHR_INT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 9B ADD_LONG vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 9C SUB_LONG vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 9D MUL_LONG vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 9E DIV_LONG vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // 9F REM_LONG vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // A0 AND_LONG vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // A1 OR_LONG vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // A2 XOR_LONG vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // A3 SHL_LONG vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // A4 SHR_LONG vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // A5 USHR_LONG vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_CORE_A | DF_CORE_B | DF_CORE_C,

  // A6 ADD_FLOAT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_FP_A | DF_FP_B | DF_FP_C,

  // A7 SUB_FLOAT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_FP_A | DF_FP_B | DF_FP_C,

  // A8 MUL_FLOAT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_FP_A | DF_FP_B | DF_FP_C,

  // A9 DIV_FLOAT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_FP_A | DF_FP_B | DF_FP_C,

  // AA REM_FLOAT vAA, vBB, vCC
  DF_DA | DF_UB | DF_UC | DF_FP_A | DF_FP_B | DF_FP_C,

  // AB ADD_DOUBLE vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_FP_A | DF_FP_B | DF_FP_C,

  // AC SUB_DOUBLE vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_FP_A | DF_FP_B | DF_FP_C,

  // AD MUL_DOUBLE vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_FP_A | DF_FP_B | DF_FP_C,

  // AE DIV_DOUBLE vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_FP_A | DF_FP_B | DF_FP_C,

  // AF REM_DOUBLE vAA, vBB, vCC
  DF_DA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_UC | DF_C_WIDE | DF_FP_A | DF_FP_B | DF_FP_C,

  // B0 ADD_INT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // B1 SUB_INT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // B2 MUL_INT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // B3 DIV_INT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // B4 REM_INT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // B5 AND_INT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // B6 OR_INT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // B7 XOR_INT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // B8 SHL_INT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // B9 SHR_INT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // BA USHR_INT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // BB ADD_LONG_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_CORE_A | DF_CORE_B,

  // BC SUB_LONG_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_CORE_A | DF_CORE_B,

  // BD MUL_LONG_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_CORE_A | DF_CORE_B,

  // BE DIV_LONG_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_CORE_A | DF_CORE_B,

  // BF REM_LONG_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_CORE_A | DF_CORE_B,

  // C0 AND_LONG_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_CORE_A | DF_CORE_B,

  // C1 OR_LONG_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_CORE_A | DF_CORE_B,

  // C2 XOR_LONG_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_CORE_A | DF_CORE_B,

  // C3 SHL_LONG_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // C4 SHR_LONG_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // C5 USHR_LONG_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_CORE_A | DF_CORE_B,

  // C6 ADD_FLOAT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_FP_A | DF_FP_B,

  // C7 SUB_FLOAT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_FP_A | DF_FP_B,

  // C8 MUL_FLOAT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_FP_A | DF_FP_B,

  // C9 DIV_FLOAT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_FP_A | DF_FP_B,

  // CA REM_FLOAT_2ADDR vA, vB
  DF_DA | DF_UA | DF_UB | DF_FP_A | DF_FP_B,

  // CB ADD_DOUBLE_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_FP_A | DF_FP_B,

  // CC SUB_DOUBLE_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_FP_A | DF_FP_B,

  // CD MUL_DOUBLE_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_FP_A | DF_FP_B,

  // CE DIV_DOUBLE_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_FP_A | DF_FP_B,

  // CF REM_DOUBLE_2ADDR vA, vB
  DF_DA | DF_A_WIDE | DF_UA | DF_UB | DF_B_WIDE | DF_FP_A | DF_FP_B,

  // D0 ADD_INT_LIT16 vA, vB, #+CCCC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // D1 RSUB_INT vA, vB, #+CCCC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // D2 MUL_INT_LIT16 vA, vB, #+CCCC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // D3 DIV_INT_LIT16 vA, vB, #+CCCC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // D4 REM_INT_LIT16 vA, vB, #+CCCC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // D5 AND_INT_LIT16 vA, vB, #+CCCC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // D6 OR_INT_LIT16 vA, vB, #+CCCC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // D7 XOR_INT_LIT16 vA, vB, #+CCCC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // D8 ADD_INT_LIT8 vAA, vBB, #+CC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // D9 RSUB_INT_LIT8 vAA, vBB, #+CC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // DA MUL_INT_LIT8 vAA, vBB, #+CC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // DB DIV_INT_LIT8 vAA, vBB, #+CC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // DC REM_INT_LIT8 vAA, vBB, #+CC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // DD AND_INT_LIT8 vAA, vBB, #+CC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // DE OR_INT_LIT8 vAA, vBB, #+CC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // DF XOR_INT_LIT8 vAA, vBB, #+CC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // E0 SHL_INT_LIT8 vAA, vBB, #+CC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // E1 SHR_INT_LIT8 vAA, vBB, #+CC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // E2 USHR_INT_LIT8 vAA, vBB, #+CC
  DF_DA | DF_UB | DF_CORE_A | DF_CORE_B,

  // E3 IGET_VOLATILE
  DF_DA | DF_UB | DF_NULL_CHK_0 | DF_REF_B,

  // E4 IPUT_VOLATILE
  DF_UA | DF_UB | DF_NULL_CHK_1 | DF_REF_B,

  // E5 SGET_VOLATILE
  DF_DA | DF_UMS,

  // E6 SPUT_VOLATILE
  DF_UA | DF_UMS,

  // E7 IGET_OBJECT_VOLATILE
  DF_DA | DF_UB | DF_NULL_CHK_0 | DF_REF_A | DF_REF_B,

  // E8 IGET_WIDE_VOLATILE
  DF_DA | DF_A_WIDE | DF_UB | DF_NULL_CHK_0 | DF_REF_B,

  // E9 IPUT_WIDE_VOLATILE
  DF_UA | DF_A_WIDE | DF_UB | DF_NULL_CHK_2 | DF_REF_B,

  // EA SGET_WIDE_VOLATILE
  DF_DA | DF_A_WIDE | DF_UMS,

  // EB SPUT_WIDE_VOLATILE
  DF_UA | DF_A_WIDE | DF_UMS,

  // EC BREAKPOINT
  DF_NOP,

  // ED THROW_VERIFICATION_ERROR
  DF_NOP | DF_UMS,

  // EE EXECUTE_INLINE
  DF_FORMAT_35C,

  // EF EXECUTE_INLINE_RANGE
  DF_FORMAT_3RC,

  // F0 INVOKE_OBJECT_INIT_RANGE
  DF_NOP | DF_NULL_CHK_0,

  // F1 RETURN_VOID_BARRIER
  DF_NOP,

  // F2 IGET_QUICK
  DF_DA | DF_UB | DF_NULL_CHK_0,

  // F3 IGET_WIDE_QUICK
  DF_DA | DF_A_WIDE | DF_UB | DF_NULL_CHK_0,

  // F4 IGET_OBJECT_QUICK
  DF_DA | DF_UB | DF_NULL_CHK_0,

  // F5 IPUT_QUICK
  DF_UA | DF_UB | DF_NULL_CHK_1,

  // F6 IPUT_WIDE_QUICK
  DF_UA | DF_A_WIDE | DF_UB | DF_NULL_CHK_2,

  // F7 IPUT_OBJECT_QUICK
  DF_UA | DF_UB | DF_NULL_CHK_1,

  // F8 INVOKE_VIRTUAL_QUICK
  DF_FORMAT_35C | DF_NULL_CHK_OUT0 | DF_UMS,

  // F9 INVOKE_VIRTUAL_QUICK_RANGE
  DF_FORMAT_3RC | DF_NULL_CHK_OUT0 | DF_UMS,

  // FA INVOKE_SUPER_QUICK
  DF_FORMAT_35C | DF_NULL_CHK_OUT0 | DF_UMS,

  // FB INVOKE_SUPER_QUICK_RANGE
  DF_FORMAT_3RC | DF_NULL_CHK_OUT0 | DF_UMS,

  // FC IPUT_OBJECT_VOLATILE
  DF_UA | DF_UB | DF_NULL_CHK_1 | DF_REF_A | DF_REF_B,

  // FD SGET_OBJECT_VOLATILE
  DF_DA | DF_REF_A | DF_UMS,

  // FE SPUT_OBJECT_VOLATILE
  DF_UA | DF_REF_A | DF_UMS,

  // FF UNUSED_FF
  DF_NOP,

  // Beginning of extended MIR opcodes
  // 100 MIR_PHI
  DF_DA | DF_NULL_TRANSFER_N,

  // 101 MIR_COPY
  DF_DA | DF_UB | DF_IS_MOVE,

  // 102 MIR_FUSED_CMPL_FLOAT
  DF_UA | DF_UB | DF_FP_A | DF_FP_B,

  // 103 MIR_FUSED_CMPG_FLOAT
  DF_UA | DF_UB | DF_FP_A | DF_FP_B,

  // 104 MIR_FUSED_CMPL_DOUBLE
  DF_UA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_FP_A | DF_FP_B,

  // 105 MIR_FUSED_CMPG_DOUBLE
  DF_UA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_FP_A | DF_FP_B,

  // 106 MIR_FUSED_CMP_LONG
  DF_UA | DF_A_WIDE | DF_UB | DF_B_WIDE | DF_CORE_A | DF_CORE_B,

  // 107 MIR_NOP
  DF_NOP,

  // 108 MIR_NULL_CHECK
  0,

  // 109 MIR_RANGE_CHECK
  0,

  // 110 MIR_DIV_ZERO_CHECK
  0,

  // 111 MIR_CHECK
  0,

  // 112 MIR_CHECKPART2
  0,

  // 113 MIR_SELECT
  DF_DA | DF_UB,
};

/* Return the base virtual register for a SSA name */
int MIRGraph::SRegToVReg(int ssa_reg) const {
  return ssa_base_vregs_->Get(ssa_reg);
}

/* Any register that is used before being defined is considered live-in */
void MIRGraph::HandleLiveInUse(ArenaBitVector* use_v, ArenaBitVector* def_v,
                            ArenaBitVector* live_in_v, int dalvik_reg_id) {
  use_v->SetBit(dalvik_reg_id);
  if (!def_v->IsBitSet(dalvik_reg_id)) {
    live_in_v->SetBit(dalvik_reg_id);
  }
}

/* Mark a reg as being defined */
void MIRGraph::HandleDef(ArenaBitVector* def_v, int dalvik_reg_id) {
  def_v->SetBit(dalvik_reg_id);
}

/*
 * Find out live-in variables for natural loops. Variables that are live-in in
 * the main loop body are considered to be defined in the entry block.
 */
bool MIRGraph::FindLocalLiveIn(BasicBlock* bb) {
  MIR* mir;
  ArenaBitVector *use_v, *def_v, *live_in_v;

  if (bb->data_flow_info == NULL) return false;

  use_v = bb->data_flow_info->use_v =
      new (arena_) ArenaBitVector(arena_, cu_->num_dalvik_registers, false, kBitMapUse);
  def_v = bb->data_flow_info->def_v =
      new (arena_) ArenaBitVector(arena_, cu_->num_dalvik_registers, false, kBitMapDef);
  live_in_v = bb->data_flow_info->live_in_v =
      new (arena_) ArenaBitVector(arena_, cu_->num_dalvik_registers, false, kBitMapLiveIn);

  for (mir = bb->first_mir_insn; mir != NULL; mir = mir->next) {
    int df_attributes = oat_data_flow_attributes_[mir->dalvikInsn.opcode];
    DecodedInstruction *d_insn = &mir->dalvikInsn;

    if (df_attributes & DF_HAS_USES) {
      if (df_attributes & DF_UA) {
        HandleLiveInUse(use_v, def_v, live_in_v, d_insn->vA);
        if (df_attributes & DF_A_WIDE) {
          HandleLiveInUse(use_v, def_v, live_in_v, d_insn->vA+1);
        }
      }
      if (df_attributes & DF_UB) {
        HandleLiveInUse(use_v, def_v, live_in_v, d_insn->vB);
        if (df_attributes & DF_B_WIDE) {
          HandleLiveInUse(use_v, def_v, live_in_v, d_insn->vB+1);
        }
      }
      if (df_attributes & DF_UC) {
        HandleLiveInUse(use_v, def_v, live_in_v, d_insn->vC);
        if (df_attributes & DF_C_WIDE) {
          HandleLiveInUse(use_v, def_v, live_in_v, d_insn->vC+1);
        }
      }
    }
    if (df_attributes & DF_FORMAT_35C) {
      for (unsigned int i = 0; i < d_insn->vA; i++) {
        HandleLiveInUse(use_v, def_v, live_in_v, d_insn->arg[i]);
      }
    }
    if (df_attributes & DF_FORMAT_3RC) {
      for (unsigned int i = 0; i < d_insn->vA; i++) {
        HandleLiveInUse(use_v, def_v, live_in_v, d_insn->vC+i);
      }
    }
    if (df_attributes & DF_HAS_DEFS) {
      HandleDef(def_v, d_insn->vA);
      if (df_attributes & DF_A_WIDE) {
        HandleDef(def_v, d_insn->vA+1);
      }
    }
  }
  return true;
}

int MIRGraph::AddNewSReg(int v_reg) {
  // Compiler temps always have a subscript of 0
  int subscript = (v_reg < 0) ? 0 : ++ssa_last_defs_[v_reg];
  int ssa_reg = GetNumSSARegs();
  SetNumSSARegs(ssa_reg + 1);
  ssa_base_vregs_->Insert(v_reg);
  ssa_subscripts_->Insert(subscript);
  DCHECK_EQ(ssa_base_vregs_->Size(), ssa_subscripts_->Size());
  return ssa_reg;
}

/* Find out the latest SSA register for a given Dalvik register */
void MIRGraph::HandleSSAUse(int* uses, int dalvik_reg, int reg_index) {
  DCHECK((dalvik_reg >= 0) && (dalvik_reg < cu_->num_dalvik_registers));
  uses[reg_index] = vreg_to_ssa_map_[dalvik_reg];
}

/* Setup a new SSA register for a given Dalvik register */
void MIRGraph::HandleSSADef(int* defs, int dalvik_reg, int reg_index) {
  DCHECK((dalvik_reg >= 0) && (dalvik_reg < cu_->num_dalvik_registers));
  int ssa_reg = AddNewSReg(dalvik_reg);
  vreg_to_ssa_map_[dalvik_reg] = ssa_reg;
  defs[reg_index] = ssa_reg;
}

/* Look up new SSA names for format_35c instructions */
void MIRGraph::DataFlowSSAFormat35C(MIR* mir) {
  DecodedInstruction *d_insn = &mir->dalvikInsn;
  int num_uses = d_insn->vA;
  int i;

  mir->ssa_rep->num_uses = num_uses;
  mir->ssa_rep->uses = static_cast<int*>(arena_->Alloc(sizeof(int) * num_uses,
                                                       ArenaAllocator::kAllocDFInfo));
  // NOTE: will be filled in during type & size inference pass
  mir->ssa_rep->fp_use = static_cast<bool*>(arena_->Alloc(sizeof(bool) * num_uses,
                                                          ArenaAllocator::kAllocDFInfo));

  for (i = 0; i < num_uses; i++) {
    HandleSSAUse(mir->ssa_rep->uses, d_insn->arg[i], i);
  }
}

/* Look up new SSA names for format_3rc instructions */
void MIRGraph::DataFlowSSAFormat3RC(MIR* mir) {
  DecodedInstruction *d_insn = &mir->dalvikInsn;
  int num_uses = d_insn->vA;
  int i;

  mir->ssa_rep->num_uses = num_uses;
  mir->ssa_rep->uses = static_cast<int*>(arena_->Alloc(sizeof(int) * num_uses,
                                                       ArenaAllocator::kAllocDFInfo));
  // NOTE: will be filled in during type & size inference pass
  mir->ssa_rep->fp_use = static_cast<bool*>(arena_->Alloc(sizeof(bool) * num_uses,
                                                          ArenaAllocator::kAllocDFInfo));

  for (i = 0; i < num_uses; i++) {
    HandleSSAUse(mir->ssa_rep->uses, d_insn->vC+i, i);
  }
}

/* Entry function to convert a block into SSA representation */
bool MIRGraph::DoSSAConversion(BasicBlock* bb) {
  MIR* mir;

  if (bb->data_flow_info == NULL) return false;

  for (mir = bb->first_mir_insn; mir != NULL; mir = mir->next) {
    mir->ssa_rep =
        static_cast<struct SSARepresentation *>(arena_->Alloc(sizeof(SSARepresentation),
                                                              ArenaAllocator::kAllocDFInfo));

    int df_attributes = oat_data_flow_attributes_[mir->dalvikInsn.opcode];

      // If not a pseudo-op, note non-leaf or can throw
    if (static_cast<int>(mir->dalvikInsn.opcode) <
        static_cast<int>(kNumPackedOpcodes)) {
      int flags = Instruction::FlagsOf(mir->dalvikInsn.opcode);

      if (flags & Instruction::kInvoke) {
        attributes_ &= ~METHOD_IS_LEAF;
      }
    }

    int num_uses = 0;

    if (df_attributes & DF_FORMAT_35C) {
      DataFlowSSAFormat35C(mir);
      continue;
    }

    if (df_attributes & DF_FORMAT_3RC) {
      DataFlowSSAFormat3RC(mir);
      continue;
    }

    if (df_attributes & DF_HAS_USES) {
      if (df_attributes & DF_UA) {
        num_uses++;
        if (df_attributes & DF_A_WIDE) {
          num_uses++;
        }
      }
      if (df_attributes & DF_UB) {
        num_uses++;
        if (df_attributes & DF_B_WIDE) {
          num_uses++;
        }
      }
      if (df_attributes & DF_UC) {
        num_uses++;
        if (df_attributes & DF_C_WIDE) {
          num_uses++;
        }
      }
    }

    if (num_uses) {
      mir->ssa_rep->num_uses = num_uses;
      mir->ssa_rep->uses = static_cast<int*>(arena_->Alloc(sizeof(int) * num_uses,
                                                           ArenaAllocator::kAllocDFInfo));
      mir->ssa_rep->fp_use = static_cast<bool*>(arena_->Alloc(sizeof(bool) * num_uses,
                                                              ArenaAllocator::kAllocDFInfo));
    }

    int num_defs = 0;

    if (df_attributes & DF_HAS_DEFS) {
      num_defs++;
      if (df_attributes & DF_A_WIDE) {
        num_defs++;
      }
    }

    if (num_defs) {
      mir->ssa_rep->num_defs = num_defs;
      mir->ssa_rep->defs = static_cast<int*>(arena_->Alloc(sizeof(int) * num_defs,
                                                           ArenaAllocator::kAllocDFInfo));
      mir->ssa_rep->fp_def = static_cast<bool*>(arena_->Alloc(sizeof(bool) * num_defs,
                                                              ArenaAllocator::kAllocDFInfo));
    }

    DecodedInstruction *d_insn = &mir->dalvikInsn;

    if (df_attributes & DF_HAS_USES) {
      num_uses = 0;
      if (df_attributes & DF_UA) {
        mir->ssa_rep->fp_use[num_uses] = df_attributes & DF_FP_A;
        HandleSSAUse(mir->ssa_rep->uses, d_insn->vA, num_uses++);
        if (df_attributes & DF_A_WIDE) {
          mir->ssa_rep->fp_use[num_uses] = df_attributes & DF_FP_A;
          HandleSSAUse(mir->ssa_rep->uses, d_insn->vA+1, num_uses++);
        }
      }
      if (df_attributes & DF_UB) {
        mir->ssa_rep->fp_use[num_uses] = df_attributes & DF_FP_B;
        HandleSSAUse(mir->ssa_rep->uses, d_insn->vB, num_uses++);
        if (df_attributes & DF_B_WIDE) {
          mir->ssa_rep->fp_use[num_uses] = df_attributes & DF_FP_B;
          HandleSSAUse(mir->ssa_rep->uses, d_insn->vB+1, num_uses++);
        }
      }
      if (df_attributes & DF_UC) {
        mir->ssa_rep->fp_use[num_uses] = df_attributes & DF_FP_C;
        HandleSSAUse(mir->ssa_rep->uses, d_insn->vC, num_uses++);
        if (df_attributes & DF_C_WIDE) {
          mir->ssa_rep->fp_use[num_uses] = df_attributes & DF_FP_C;
          HandleSSAUse(mir->ssa_rep->uses, d_insn->vC+1, num_uses++);
        }
      }
    }
    if (df_attributes & DF_HAS_DEFS) {
      mir->ssa_rep->fp_def[0] = df_attributes & DF_FP_A;
      HandleSSADef(mir->ssa_rep->defs, d_insn->vA, 0);
      if (df_attributes & DF_A_WIDE) {
        mir->ssa_rep->fp_def[1] = df_attributes & DF_FP_A;
        HandleSSADef(mir->ssa_rep->defs, d_insn->vA+1, 1);
      }
    }
  }

  /*
   * Take a snapshot of Dalvik->SSA mapping at the end of each block. The
   * input to PHI nodes can be derived from the snapshot of all
   * predecessor blocks.
   */
  bb->data_flow_info->vreg_to_ssa_map =
      static_cast<int*>(arena_->Alloc(sizeof(int) * cu_->num_dalvik_registers,
                                      ArenaAllocator::kAllocDFInfo));

  memcpy(bb->data_flow_info->vreg_to_ssa_map, vreg_to_ssa_map_,
         sizeof(int) * cu_->num_dalvik_registers);
  return true;
}

/* Setup the basic data structures for SSA conversion */
void MIRGraph::CompilerInitializeSSAConversion() {
  size_t num_dalvik_reg = cu_->num_dalvik_registers;

  ssa_base_vregs_ = new (arena_) GrowableArray<int>(arena_, num_dalvik_reg + GetDefCount() + 128,
                                                    kGrowableArraySSAtoDalvikMap);
  ssa_subscripts_ = new (arena_) GrowableArray<int>(arena_, num_dalvik_reg + GetDefCount() + 128,
                                                    kGrowableArraySSAtoDalvikMap);
  /*
   * Initial number of SSA registers is equal to the number of Dalvik
   * registers.
   */
  SetNumSSARegs(num_dalvik_reg);

  /*
   * Initialize the SSA2Dalvik map list. For the first num_dalvik_reg elements,
   * the subscript is 0 so we use the ENCODE_REG_SUB macro to encode the value
   * into "(0 << 16) | i"
   */
  for (unsigned int i = 0; i < num_dalvik_reg; i++) {
    ssa_base_vregs_->Insert(i);
    ssa_subscripts_->Insert(0);
  }

  /*
   * Initialize the DalvikToSSAMap map. There is one entry for each
   * Dalvik register, and the SSA names for those are the same.
   */
  vreg_to_ssa_map_ =
      static_cast<int*>(arena_->Alloc(sizeof(int) * num_dalvik_reg,
                                      ArenaAllocator::kAllocDFInfo));
  /* Keep track of the higest def for each dalvik reg */
  ssa_last_defs_ =
      static_cast<int*>(arena_->Alloc(sizeof(int) * num_dalvik_reg,
                                      ArenaAllocator::kAllocDFInfo));

  for (unsigned int i = 0; i < num_dalvik_reg; i++) {
    vreg_to_ssa_map_[i] = i;
    ssa_last_defs_[i] = 0;
  }

  /* Add ssa reg for Method* */
  method_sreg_ = AddNewSReg(SSA_METHOD_BASEREG);

  /*
   * Allocate the BasicBlockDataFlow structure for the entry and code blocks
   */
  GrowableArray<BasicBlock*>::Iterator iterator(&block_list_);

  while (true) {
    BasicBlock* bb = iterator.Next();
    if (bb == NULL) break;
    if (bb->hidden == true) continue;
    if (bb->block_type == kDalvikByteCode ||
      bb->block_type == kEntryBlock ||
      bb->block_type == kExitBlock) {
      bb->data_flow_info =
          static_cast<BasicBlockDataFlow*>(arena_->Alloc(sizeof(BasicBlockDataFlow),
                                                         ArenaAllocator::kAllocDFInfo));
      }
  }
}

/*
 * This function will make a best guess at whether the invoke will
 * end up using Method*.  It isn't critical to get it exactly right,
 * and attempting to do would involve more complexity than it's
 * worth.
 */
bool MIRGraph::InvokeUsesMethodStar(MIR* mir) {
  InvokeType type;
  Instruction::Code opcode = mir->dalvikInsn.opcode;
  switch (opcode) {
    case Instruction::INVOKE_STATIC:
    case Instruction::INVOKE_STATIC_RANGE:
      type = kStatic;
      break;
    case Instruction::INVOKE_DIRECT:
    case Instruction::INVOKE_DIRECT_RANGE:
      type = kDirect;
      break;
    case Instruction::INVOKE_VIRTUAL:
    case Instruction::INVOKE_VIRTUAL_RANGE:
      type = kVirtual;
      break;
    case Instruction::INVOKE_INTERFACE:
    case Instruction::INVOKE_INTERFACE_RANGE:
      return false;
    case Instruction::INVOKE_SUPER_RANGE:
    case Instruction::INVOKE_SUPER:
      type = kSuper;
      break;
    default:
      LOG(WARNING) << "Unexpected invoke op: " << opcode;
      return false;
  }
  DexCompilationUnit m_unit(cu_);
  MethodReference target_method(cu_->dex_file, mir->dalvikInsn.vB);
  int vtable_idx;
  uintptr_t direct_code;
  uintptr_t direct_method;
  uint32_t current_offset = static_cast<uint32_t>(current_offset_);
  bool fast_path =
      cu_->compiler_driver->ComputeInvokeInfo(&m_unit, current_offset,
                                              type, target_method,
                                              vtable_idx,
                                              direct_code, direct_method,
                                              false) &&
                                              !(cu_->enable_debug & (1 << kDebugSlowInvokePath));
  return (((type == kDirect) || (type == kStatic)) &&
          fast_path && ((direct_code == 0) || (direct_method == 0)));
}

/*
 * Count uses, weighting by loop nesting depth.  This code only
 * counts explicitly used s_regs.  A later phase will add implicit
 * counts for things such as Method*, null-checked references, etc.
 */
bool MIRGraph::CountUses(struct BasicBlock* bb) {
  if (bb->block_type != kDalvikByteCode) {
    return false;
  }
  for (MIR* mir = bb->first_mir_insn; (mir != NULL); mir = mir->next) {
    if (mir->ssa_rep == NULL) {
      continue;
    }
    uint32_t weight = std::min(16U, static_cast<uint32_t>(bb->nesting_depth));
    for (int i = 0; i < mir->ssa_rep->num_uses; i++) {
      int s_reg = mir->ssa_rep->uses[i];
      raw_use_counts_.Increment(s_reg);
      use_counts_.Put(s_reg, use_counts_.Get(s_reg) + (1 << weight));
    }
    if (!(cu_->disable_opt & (1 << kPromoteCompilerTemps))) {
      int df_attributes = oat_data_flow_attributes_[mir->dalvikInsn.opcode];
      // Implicit use of Method* ? */
      if (df_attributes & DF_UMS) {
        /*
         * Some invokes will not use Method* - need to perform test similar
         * to that found in GenInvoke() to decide whether to count refs
         * for Method* on invoke-class opcodes.
         * TODO: refactor for common test here, save results for GenInvoke
         */
        int uses_method_star = true;
        if ((df_attributes & (DF_FORMAT_35C | DF_FORMAT_3RC)) &&
            !(df_attributes & DF_NON_NULL_RET)) {
          uses_method_star &= InvokeUsesMethodStar(mir);
        }
        if (uses_method_star) {
          raw_use_counts_.Increment(method_sreg_);
          use_counts_.Put(method_sreg_, use_counts_.Get(method_sreg_) + (1 << weight));
        }
      }
    }
  }
  return false;
}

void MIRGraph::MethodUseCount() {
  // Now that we know, resize the lists.
  int num_ssa_regs = GetNumSSARegs();
  use_counts_.Resize(num_ssa_regs + 32);
  raw_use_counts_.Resize(num_ssa_regs + 32);
  // Initialize list
  for (int i = 0; i < num_ssa_regs; i++) {
    use_counts_.Insert(0);
    raw_use_counts_.Insert(0);
  }
  if (cu_->disable_opt & (1 << kPromoteRegs)) {
    return;
  }
  AllNodesIterator iter(this, false /* not iterative */);
  for (BasicBlock* bb = iter.Next(); bb != NULL; bb = iter.Next()) {
    CountUses(bb);
  }
}

/* Verify if all the successor is connected with all the claimed predecessors */
bool MIRGraph::VerifyPredInfo(BasicBlock* bb) {
  GrowableArray<BasicBlock*>::Iterator iter(bb->predecessors);

  while (true) {
    BasicBlock *pred_bb = iter.Next();
    if (!pred_bb) break;
    bool found = false;
    if (pred_bb->taken == bb) {
        found = true;
    } else if (pred_bb->fall_through == bb) {
        found = true;
    } else if (pred_bb->successor_block_list.block_list_type != kNotUsed) {
      GrowableArray<SuccessorBlockInfo*>::Iterator iterator(pred_bb->successor_block_list.blocks);
      while (true) {
        SuccessorBlockInfo *successor_block_info = iterator.Next();
        if (successor_block_info == NULL) break;
        BasicBlock *succ_bb = successor_block_info->block;
        if (succ_bb == bb) {
            found = true;
            break;
        }
      }
    }
    if (found == false) {
      char block_name1[BLOCK_NAME_LEN], block_name2[BLOCK_NAME_LEN];
      GetBlockName(bb, block_name1);
      GetBlockName(pred_bb, block_name2);
      DumpCFG("/sdcard/cfg/", false);
      LOG(FATAL) << "Successor " << block_name1 << "not found from "
                 << block_name2;
    }
  }
  return true;
}

void MIRGraph::VerifyDataflow() {
    /* Verify if all blocks are connected as claimed */
  AllNodesIterator iter(this, false /* not iterative */);
  for (BasicBlock* bb = iter.Next(); bb != NULL; bb = iter.Next()) {
    VerifyPredInfo(bb);
  }
}

}  // namespace art
