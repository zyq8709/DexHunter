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

#include <inttypes.h>

/*
 * Dump the fixed-purpose MIPS registers, along with some other info.
 *
 */
void dvmMterpDumpMipsRegs(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    register uint32_t rPC       asm("s0");
    register uint32_t rFP       asm("s1");
    register uint32_t rSELF     asm("s2");
    register uint32_t rIBASE    asm("s3");
    register uint32_t rINST     asm("s4");
    register uint32_t rOBJ      asm("s5");
    register uint32_t rBIX      asm("s6");
    register uint32_t rTEMP	asm("s7");

    //extern char dvmAsmInstructionStart[];

    printf("REGS: a0=%08x a1=%08x a2=%08x a3=%08x\n", a0, a1, a2, a3);
    printf("    : rPC=%08x rFP=%08x rSELF=%08x rIBASE=%08x\n",
        rPC, rFP, rSELF, rIBASE);
    printf("    : rINST=%08x rOBJ=%08x rBIX=%08x rTEMP=%08x \n", rINST, rOBJ, rBIX, rTEMP);

    //Thread* self = (Thread*) rSELF;
    //const Method* method = self->method;
    printf("    + self is %p\n", dvmThreadSelf());
    //printf("    + currently in %s.%s %s\n",
    //    method->clazz->descriptor, method->name, method->signature);
    //printf("    + dvmAsmInstructionStart = %p\n", dvmAsmInstructionStart);
    //printf("    + next handler for 0x%02x = %p\n",
    //    rINST & 0xff, dvmAsmInstructionStart + (rINST & 0xff) * 64);
}

/*
 * Dump the StackSaveArea for the specified frame pointer.
 */
void dvmDumpFp(void* fp, StackSaveArea* otherSaveArea)
{
    StackSaveArea* saveArea = SAVEAREA_FROM_FP(fp);
    printf("StackSaveArea for fp %p [%p/%p]:\n", fp, saveArea, otherSaveArea);
#ifdef EASY_GDB
    printf("  prevSave=%p, prevFrame=%p savedPc=%p meth=%p curPc=%p\n",
        saveArea->prevSave, saveArea->prevFrame, saveArea->savedPc,
        saveArea->method, saveArea->xtra.currentPc);
#else
    printf("  prevFrame=%p savedPc=%p meth=%p curPc=%p fp[0]=0x%08x\n",
        saveArea->prevFrame, saveArea->savedPc,
        saveArea->method, saveArea->xtra.currentPc,
        *(u4*)fp);
#endif
}

/*
 * Does the bulk of the work for common_printMethod().
 */
void dvmMterpPrintMethod(Method* method)
{
    /*
     * It is a direct (non-virtual) method if it is static, private,
     * or a constructor.
     */
    bool isDirect =
        ((method->accessFlags & (ACC_STATIC|ACC_PRIVATE)) != 0) ||
        (method->name[0] == '<');

    char* desc = dexProtoCopyMethodDescriptor(&method->prototype);

    printf("<%c:%s.%s %s> ",
            isDirect ? 'D' : 'V',
            method->clazz->descriptor,
            method->name,
            desc);

    free(desc);
}
