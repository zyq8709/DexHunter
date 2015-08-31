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

/*
 * This file contains codegen for the Thumb ISA and is intended to be
 * includes by:
 *
 *        Codegen-$(TARGET_ARCH_VARIANT).c
 *
 */

/*
 * Alloc a pair of core registers, or a double.  Low reg in low byte,
 * high reg in next byte.
 */
int dvmCompilerAllocTypedTempPair(CompilationUnit *cUnit, bool fpHint,
                                  int regClass)
{
    int highReg;
    int lowReg;
    int res = 0;
    lowReg = dvmCompilerAllocTemp(cUnit);
    highReg = dvmCompilerAllocTemp(cUnit);
    res = (lowReg & 0xff) | ((highReg & 0xff) << 8);
    return res;
}

int dvmCompilerAllocTypedTemp(CompilationUnit *cUnit, bool fpHint, int regClass)
{
    return dvmCompilerAllocTemp(cUnit);
}
