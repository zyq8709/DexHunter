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
 * Dalvik instruction utility functions.
 *
 * IMPORTANT NOTE: Much of the contents of this file are generated
 * automatically by the opcode-gen tool. Any edits to the generated
 * sections will get wiped out the next time the tool is run.
 */

#include "InstrUtils.h"
#include <stdlib.h>

/*
 * Table that maps each opcode to the full width of instructions that
 * use that opcode, in (16-bit) code units. Unimplemented opcodes as
 * well as the "breakpoint" opcode have a width of zero.
 */
static InstructionWidth gInstructionWidthTable[kNumPackedOpcodes] = {
    // BEGIN(libdex-widths); GENERATED AUTOMATICALLY BY opcode-gen
    1, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 2, 3, 2, 2, 3, 5, 2, 2, 3, 2, 1, 1, 2,
    2, 1, 2, 2, 3, 3, 3, 1, 1, 2, 3, 3, 3, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0,
    0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3,
    3, 3, 3, 0, 3, 3, 3, 3, 3, 0, 0, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 2, 3, 3,
    3, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 2, 2, 2, 0,
    // END(libdex-widths)
};

/*
 * Table that maps each opcode to the flags associated with that
 * opcode.
 */
static u1 gOpcodeFlagsTable[kNumPackedOpcodes] = {
    // BEGIN(libdex-flags); GENERATED AUTOMATICALLY BY opcode-gen
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanReturn,
    kInstrCanReturn,
    kInstrCanReturn,
    kInstrCanReturn,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue,
    kInstrCanThrow,
    kInstrCanBranch,
    kInstrCanBranch,
    kInstrCanBranch,
    kInstrCanContinue|kInstrCanSwitch,
    kInstrCanContinue|kInstrCanSwitch,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue|kInstrCanBranch,
    kInstrCanContinue|kInstrCanBranch,
    kInstrCanContinue|kInstrCanBranch,
    kInstrCanContinue|kInstrCanBranch,
    kInstrCanContinue|kInstrCanBranch,
    kInstrCanContinue|kInstrCanBranch,
    kInstrCanContinue|kInstrCanBranch,
    kInstrCanContinue|kInstrCanBranch,
    kInstrCanContinue|kInstrCanBranch,
    kInstrCanContinue|kInstrCanBranch,
    kInstrCanContinue|kInstrCanBranch,
    kInstrCanContinue|kInstrCanBranch,
    0,
    0,
    0,
    0,
    0,
    0,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    0,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    0,
    0,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    0,
    kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanReturn,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanContinue|kInstrCanThrow|kInstrInvoke,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    kInstrCanContinue|kInstrCanThrow,
    0,
    // END(libdex-flags)
};

/*
 * Table that maps each opcode to the instruction format associated
 * that opcode.
 */
static u1 gInstructionFormatTable[kNumPackedOpcodes] = {
    // BEGIN(libdex-formats); GENERATED AUTOMATICALLY BY opcode-gen
    kFmt10x,  kFmt12x,  kFmt22x,  kFmt32x,  kFmt12x,  kFmt22x,  kFmt32x,
    kFmt12x,  kFmt22x,  kFmt32x,  kFmt11x,  kFmt11x,  kFmt11x,  kFmt11x,
    kFmt10x,  kFmt11x,  kFmt11x,  kFmt11x,  kFmt11n,  kFmt21s,  kFmt31i,
    kFmt21h,  kFmt21s,  kFmt31i,  kFmt51l,  kFmt21h,  kFmt21c,  kFmt31c,
    kFmt21c,  kFmt11x,  kFmt11x,  kFmt21c,  kFmt22c,  kFmt12x,  kFmt21c,
    kFmt22c,  kFmt35c,  kFmt3rc,  kFmt31t,  kFmt11x,  kFmt10t,  kFmt20t,
    kFmt30t,  kFmt31t,  kFmt31t,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,
    kFmt23x,  kFmt22t,  kFmt22t,  kFmt22t,  kFmt22t,  kFmt22t,  kFmt22t,
    kFmt21t,  kFmt21t,  kFmt21t,  kFmt21t,  kFmt21t,  kFmt21t,  kFmt00x,
    kFmt00x,  kFmt00x,  kFmt00x,  kFmt00x,  kFmt00x,  kFmt23x,  kFmt23x,
    kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,
    kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt22c,  kFmt22c,
    kFmt22c,  kFmt22c,  kFmt22c,  kFmt22c,  kFmt22c,  kFmt22c,  kFmt22c,
    kFmt22c,  kFmt22c,  kFmt22c,  kFmt22c,  kFmt22c,  kFmt21c,  kFmt21c,
    kFmt21c,  kFmt21c,  kFmt21c,  kFmt21c,  kFmt21c,  kFmt21c,  kFmt21c,
    kFmt21c,  kFmt21c,  kFmt21c,  kFmt21c,  kFmt21c,  kFmt35c,  kFmt35c,
    kFmt35c,  kFmt35c,  kFmt35c,  kFmt00x,  kFmt3rc,  kFmt3rc,  kFmt3rc,
    kFmt3rc,  kFmt3rc,  kFmt00x,  kFmt00x,  kFmt12x,  kFmt12x,  kFmt12x,
    kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,
    kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,
    kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt23x,  kFmt23x,  kFmt23x,
    kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,
    kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,
    kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,
    kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,  kFmt23x,
    kFmt23x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,
    kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,
    kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,
    kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,
    kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt12x,  kFmt22s,  kFmt22s,
    kFmt22s,  kFmt22s,  kFmt22s,  kFmt22s,  kFmt22s,  kFmt22s,  kFmt22b,
    kFmt22b,  kFmt22b,  kFmt22b,  kFmt22b,  kFmt22b,  kFmt22b,  kFmt22b,
    kFmt22b,  kFmt22b,  kFmt22b,  kFmt22c,  kFmt22c,  kFmt21c,  kFmt21c,
    kFmt22c,  kFmt22c,  kFmt22c,  kFmt21c,  kFmt21c,  kFmt00x,  kFmt20bc,
    kFmt35mi, kFmt3rmi, kFmt35c,  kFmt10x,  kFmt22cs, kFmt22cs, kFmt22cs,
    kFmt22cs, kFmt22cs, kFmt22cs, kFmt35ms, kFmt3rms, kFmt35ms, kFmt3rms,
    kFmt22c,  kFmt21c,  kFmt21c,  kFmt00x,
    // END(libdex-formats)
};

/*
 * Table that maps each opcode to the index type implied by that
 * opcode.
 */
static u1 gInstructionIndexTypeTable[kNumPackedOpcodes] = {
    // BEGIN(libdex-index-types); GENERATED AUTOMATICALLY BY opcode-gen
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexStringRef,
    kIndexStringRef,    kIndexTypeRef,      kIndexNone,
    kIndexNone,         kIndexTypeRef,      kIndexTypeRef,
    kIndexNone,         kIndexTypeRef,      kIndexTypeRef,
    kIndexTypeRef,      kIndexTypeRef,      kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexUnknown,
    kIndexUnknown,      kIndexUnknown,      kIndexUnknown,
    kIndexUnknown,      kIndexUnknown,      kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexFieldRef,     kIndexFieldRef,
    kIndexFieldRef,     kIndexFieldRef,     kIndexFieldRef,
    kIndexFieldRef,     kIndexFieldRef,     kIndexFieldRef,
    kIndexFieldRef,     kIndexFieldRef,     kIndexFieldRef,
    kIndexFieldRef,     kIndexFieldRef,     kIndexFieldRef,
    kIndexFieldRef,     kIndexFieldRef,     kIndexFieldRef,
    kIndexFieldRef,     kIndexFieldRef,     kIndexFieldRef,
    kIndexFieldRef,     kIndexFieldRef,     kIndexFieldRef,
    kIndexFieldRef,     kIndexFieldRef,     kIndexFieldRef,
    kIndexFieldRef,     kIndexFieldRef,     kIndexMethodRef,
    kIndexMethodRef,    kIndexMethodRef,    kIndexMethodRef,
    kIndexMethodRef,    kIndexUnknown,      kIndexMethodRef,
    kIndexMethodRef,    kIndexMethodRef,    kIndexMethodRef,
    kIndexMethodRef,    kIndexUnknown,      kIndexUnknown,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexNone,
    kIndexNone,         kIndexNone,         kIndexFieldRef,
    kIndexFieldRef,     kIndexFieldRef,     kIndexFieldRef,
    kIndexFieldRef,     kIndexFieldRef,     kIndexFieldRef,
    kIndexFieldRef,     kIndexFieldRef,     kIndexUnknown,
    kIndexVaries,       kIndexInlineMethod, kIndexInlineMethod,
    kIndexMethodRef,    kIndexNone,         kIndexFieldOffset,
    kIndexFieldOffset,  kIndexFieldOffset,  kIndexFieldOffset,
    kIndexFieldOffset,  kIndexFieldOffset,  kIndexVtableOffset,
    kIndexVtableOffset, kIndexVtableOffset, kIndexVtableOffset,
    kIndexFieldRef,     kIndexFieldRef,     kIndexFieldRef,
    kIndexUnknown,
    // END(libdex-index-types)
};

/*
 * Global InstructionInfoTables struct.
 */
InstructionInfoTables gDexOpcodeInfo = {
    gInstructionFormatTable,
    gInstructionIndexTypeTable,
    gOpcodeFlagsTable,
    gInstructionWidthTable
};

/*
 * Handy macros for helping decode instructions.
 */
#define FETCH(_offset)      (insns[(_offset)])
#define FETCH_u4(_offset)   (fetch_u4_impl((_offset), insns))
#define INST_A(_inst)       (((u2)(_inst) >> 8) & 0x0f)
#define INST_B(_inst)       ((u2)(_inst) >> 12)
#define INST_AA(_inst)      ((_inst) >> 8)

/* Helper for FETCH_u4, above. */
static inline u4 fetch_u4_impl(u4 offset, const u2* insns) {
    return insns[offset] | ((u4) insns[offset+1] << 16);
}

/*
 * Decode the instruction pointed to by "insns".
 *
 * Fills out the pieces of "pDec" that are affected by the current
 * instruction.  Does not touch anything else.
 */
void dexDecodeInstruction(const u2* insns, DecodedInstruction* pDec)
{
    u2 inst = *insns;
    Opcode opcode = dexOpcodeFromCodeUnit(inst);
    InstructionFormat format = dexGetFormatFromOpcode(opcode);

    pDec->opcode = opcode;
    pDec->indexType = dexGetIndexTypeFromOpcode(opcode);

    switch (format) {
    case kFmt10x:       // op
        /* nothing to do; copy the AA bits out for the verifier */
        pDec->vA = INST_AA(inst);
        break;
    case kFmt12x:       // op vA, vB
        pDec->vA = INST_A(inst);
        pDec->vB = INST_B(inst);
        break;
    case kFmt11n:       // op vA, #+B
        pDec->vA = INST_A(inst);
        pDec->vB = (s4) (INST_B(inst) << 28) >> 28; // sign extend 4-bit value
        break;
    case kFmt11x:       // op vAA
        pDec->vA = INST_AA(inst);
        break;
    case kFmt10t:       // op +AA
        pDec->vA = (s1) INST_AA(inst);              // sign-extend 8-bit value
        break;
    case kFmt20t:       // op +AAAA
        pDec->vA = (s2) FETCH(1);                   // sign-extend 16-bit value
        break;
    case kFmt20bc:      // [opt] op AA, thing@BBBB
    case kFmt21c:       // op vAA, thing@BBBB
    case kFmt22x:       // op vAA, vBBBB
        pDec->vA = INST_AA(inst);
        pDec->vB = FETCH(1);
        break;
    case kFmt21s:       // op vAA, #+BBBB
    case kFmt21t:       // op vAA, +BBBB
        pDec->vA = INST_AA(inst);
        pDec->vB = (s2) FETCH(1);                   // sign-extend 16-bit value
        break;
    case kFmt21h:       // op vAA, #+BBBB0000[00000000]
        pDec->vA = INST_AA(inst);
        /*
         * The value should be treated as right-zero-extended, but we don't
         * actually do that here. Among other things, we don't know if it's
         * the top bits of a 32- or 64-bit value.
         */
        pDec->vB = FETCH(1);
        break;
    case kFmt23x:       // op vAA, vBB, vCC
        pDec->vA = INST_AA(inst);
        pDec->vB = FETCH(1) & 0xff;
        pDec->vC = FETCH(1) >> 8;
        break;
    case kFmt22b:       // op vAA, vBB, #+CC
        pDec->vA = INST_AA(inst);
        pDec->vB = FETCH(1) & 0xff;
        pDec->vC = (s1) (FETCH(1) >> 8);            // sign-extend 8-bit value
        break;
    case kFmt22s:       // op vA, vB, #+CCCC
    case kFmt22t:       // op vA, vB, +CCCC
        pDec->vA = INST_A(inst);
        pDec->vB = INST_B(inst);
        pDec->vC = (s2) FETCH(1);                   // sign-extend 16-bit value
        break;
    case kFmt22c:       // op vA, vB, thing@CCCC
    case kFmt22cs:      // [opt] op vA, vB, field offset CCCC
        pDec->vA = INST_A(inst);
        pDec->vB = INST_B(inst);
        pDec->vC = FETCH(1);
        break;
    case kFmt30t:       // op +AAAAAAAA
        pDec->vA = FETCH_u4(1);                     // signed 32-bit value
        break;
    case kFmt31t:       // op vAA, +BBBBBBBB
    case kFmt31c:       // op vAA, string@BBBBBBBB
        pDec->vA = INST_AA(inst);
        pDec->vB = FETCH_u4(1);                     // 32-bit value
        break;
    case kFmt32x:       // op vAAAA, vBBBB
        pDec->vA = FETCH(1);
        pDec->vB = FETCH(2);
        break;
    case kFmt31i:       // op vAA, #+BBBBBBBB
        pDec->vA = INST_AA(inst);
        pDec->vB = FETCH_u4(1);                     // signed 32-bit value
        break;
    case kFmt35c:       // op {vC, vD, vE, vF, vG}, thing@BBBB
    case kFmt35ms:      // [opt] invoke-virtual+super
    case kFmt35mi:      // [opt] inline invoke
        {
            /*
             * Note that the fields mentioned in the spec don't appear in
             * their "usual" positions here compared to most formats. This
             * was done so that the field names for the argument count and
             * reference index match between this format and the corresponding
             * range formats (3rc and friends).
             *
             * Bottom line: The argument count is always in vA, and the
             * method constant (or equivalent) is always in vB.
             */
            u2 regList;
            int i, count;

            pDec->vA = INST_B(inst); // This is labeled A in the spec.
            pDec->vB = FETCH(1);
            regList = FETCH(2);

            count = pDec->vA;

            /*
             * Copy the argument registers into the arg[] array, and
             * also copy the first argument (if any) into vC. (The
             * DecodedInstruction structure doesn't have separate
             * fields for {vD, vE, vF, vG}, so there's no need to make
             * copies of those.) Note that cases 5..2 fall through.
             */
            switch (count) {
            case 5: {
                if (format == kFmt35mi) {
                    /* A fifth arg is verboten for inline invokes. */
                    ALOGW("Invalid arg count in 35mi (5)");
                    goto bail;
                }
                /*
                 * Per note at the top of this format decoder, the
                 * fifth argument comes from the A field in the
                 * instruction, but it's labeled G in the spec.
                 */
                pDec->arg[4] = INST_A(inst);
            }
            case 4: pDec->arg[3] = (regList >> 12) & 0x0f;
            case 3: pDec->arg[2] = (regList >> 8) & 0x0f;
            case 2: pDec->arg[1] = (regList >> 4) & 0x0f;
            case 1: pDec->vC = pDec->arg[0] = regList & 0x0f; break;
            case 0: break; // Valid, but no need to do anything.
            default:
                ALOGW("Invalid arg count in 35c/35ms/35mi (%d)", count);
                goto bail;
            }
        }
        break;
    case kFmt3rc:       // op {vCCCC .. v(CCCC+AA-1)}, meth@BBBB
    case kFmt3rms:      // [opt] invoke-virtual+super/range
    case kFmt3rmi:      // [opt] execute-inline/range
        pDec->vA = INST_AA(inst);
        pDec->vB = FETCH(1);
        pDec->vC = FETCH(2);
        break;
    case kFmt51l:       // op vAA, #+BBBBBBBBBBBBBBBB
        pDec->vA = INST_AA(inst);
        pDec->vB_wide = FETCH_u4(1) | ((u8) FETCH_u4(3) << 32);
        break;
    default:
        ALOGW("Can't decode unexpected format %d (op=%d)", format, opcode);
        assert(false);
        break;
    }

bail:
    ;
}

/*
 * Return the width of the specified instruction, or 0 if not defined.  Also
 * works for special OP_NOP entries, including switch statement data tables
 * and array data.
 */
size_t dexGetWidthFromInstruction(const u2* insns)
{
    size_t width;

    if (*insns == kPackedSwitchSignature) {
        width = 4 + insns[1] * 2;
    } else if (*insns == kSparseSwitchSignature) {
        width = 2 + insns[1] * 4;
    } else if (*insns == kArrayDataSignature) {
        u2 elemWidth = insns[1];
        u4 len = insns[2] | (((u4)insns[3]) << 16);
        // The plus 1 is to round up for odd size and width.
        width = 4 + (elemWidth * len + 1) / 2;
    } else {
        width = dexGetWidthFromOpcode(dexOpcodeFromCodeUnit(insns[0]));
    }

    return width;
}
