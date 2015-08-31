/*
 * This file was generated automatically by gen-template.py for 'ia32'.
 *
 * --> DO NOT EDIT <--
 */

/* File: ia32/header.S */
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

#if defined(WITH_JIT)

/* Subset of defines from mterp/x86/header.S */
#define rSELF (%ebp)
#define rPC   %esi
#define rFP   %edi
#define rINST %ebx

/*
 * This is a #include, not a %include, because we want the C pre-processor
 * to expand the macros into assembler assignment statements.
 */
#include "../../../mterp/common/asm-constants.h"

/* File: ia32/platform.S */
/*
 * ===========================================================================
 *  CPU-version-specific defines and utility
 * ===========================================================================
 */




    .global dvmCompilerTemplateStart
    .type   dvmCompilerTemplateStart, %function
    .section .data.rel.ro

dvmCompilerTemplateStart:

/* ------------------------------ */
    .balign 4
    .global dvmCompiler_TEMPLATE_INTERPRET
dvmCompiler_TEMPLATE_INTERPRET:
/* File: ia32/TEMPLATE_INTERPRET.S */
    /*
     * This handler is a bit odd - it may be called via chaining or
     * from static code and is expected to cause control to flow
     * to the interpreter.  The problem is where to find the Dalvik
     * PC of the next instruction.  When called via chaining, the dPC
     * will be located at *rp.  When called from static code, rPC is
     * valid and rp is a real return pointer (that should be ignored).
     * The Arm target deals with this by using the link register as
     * a flag.  If it is zero, we know we were called from static code.
     * If non-zero, it points to the chain cell containing dPC.
     * For x86, we'll infer the source by looking where rp points.
     * If it points to anywhere within the code cache, we'll assume
     * we got here via chaining.  Otherwise, we'll assume rPC is valid.
     *
     * On entry:
     *    (TOS)<- return pointer or pointer to dPC
     */

/*
 * FIXME - this won't work as-is.  The cache boundaries are not
 * set up until later.  Perhaps rething this whole thing.  Do we
 * really need an interpret teplate?
 */


     movl   rSELF,%ecx
     movl   $.LinterpPunt,%edx
     pop    %eax
     /*cmpl   %eax,offThread_jitCacheEnd(%ecx)*/
     ja     1f
     /*cmpl   %eax,offThread_jitCacheStart(%ecx)*/
     jb     1f
     movl   %eax,rPC
1:
     jmp    *(%edx)

.LinterpPunt:
    .long   dvmJitToInterpPunt

    .size   dvmCompilerTemplateStart, .-dvmCompilerTemplateStart
/* File: ia32/footer.S */
/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .section .data.rel.ro
    .align  4

    .global dvmCompilerTemplateEnd
dvmCompilerTemplateEnd:

#endif /* WITH_JIT */

