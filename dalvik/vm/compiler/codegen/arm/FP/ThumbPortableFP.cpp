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

/* Forward-declare the portable versions due to circular dependency */
static bool genArithOpFloatPortable(CompilationUnit *cUnit, MIR *mir,
                                    RegLocation rlDest, RegLocation rlSrc1,
                                    RegLocation rlSrc2);

static bool genArithOpDoublePortable(CompilationUnit *cUnit, MIR *mir,
                                     RegLocation rlDest, RegLocation rlSrc1,
                                     RegLocation rlSrc2);

static bool genConversionPortable(CompilationUnit *cUnit, MIR *mir);

static bool handleExecuteInlineC(CompilationUnit *cUnit, MIR *mir);

static bool genConversion(CompilationUnit *cUnit, MIR *mir)
{
    return genConversionPortable(cUnit, mir);
}

static bool genArithOpFloat(CompilationUnit *cUnit, MIR *mir,
                            RegLocation rlDest, RegLocation rlSrc1,
                            RegLocation rlSrc2)
{
    return genArithOpFloatPortable(cUnit, mir, rlDest, rlSrc1, rlSrc2);
}

static bool genArithOpDouble(CompilationUnit *cUnit, MIR *mir,
                             RegLocation rlDest, RegLocation rlSrc1,
                             RegLocation rlSrc2)
{
    return genArithOpDoublePortable(cUnit, mir, rlDest, rlSrc1, rlSrc2);
}

static bool genInlineSqrt(CompilationUnit *cUnit, MIR *mir)
{
    return handleExecuteInlineC(cUnit, mir);
}

static bool genCmpFP(CompilationUnit *cUnit, MIR *mir, RegLocation rlDest,
                     RegLocation rlSrc1, RegLocation rlSrc2)
{
    RegLocation rlResult = LOC_C_RETURN;
    /*
     * Don't attempt to optimize register usage since these opcodes call out to
     * the handlers.
     */
    switch (mir->dalvikInsn.opcode) {
        case OP_CMPL_FLOAT:
            loadValueDirectFixed(cUnit, rlSrc1, r0);
            loadValueDirectFixed(cUnit, rlSrc2, r1);
            genDispatchToHandler(cUnit, TEMPLATE_CMPL_FLOAT);
            storeValue(cUnit, rlDest, rlResult);
            break;
        case OP_CMPG_FLOAT:
            loadValueDirectFixed(cUnit, rlSrc1, r0);
            loadValueDirectFixed(cUnit, rlSrc2, r1);
            genDispatchToHandler(cUnit, TEMPLATE_CMPG_FLOAT);
            storeValue(cUnit, rlDest, rlResult);
            break;
        case OP_CMPL_DOUBLE:
            loadValueDirectWideFixed(cUnit, rlSrc1, r0, r1);
            loadValueDirectWideFixed(cUnit, rlSrc2, r2, r3);
            genDispatchToHandler(cUnit, TEMPLATE_CMPL_DOUBLE);
            storeValue(cUnit, rlDest, rlResult);
            break;
        case OP_CMPG_DOUBLE:
            loadValueDirectWideFixed(cUnit, rlSrc1, r0, r1);
            loadValueDirectWideFixed(cUnit, rlSrc2, r2, r3);
            genDispatchToHandler(cUnit, TEMPLATE_CMPG_DOUBLE);
            storeValue(cUnit, rlDest, rlResult);
            break;
        default:
            return true;
    }
    return false;
}
