#include <inttypes.h>

/*
 * Dump the fixed-purpose ARM registers, along with some other info.
 *
 * This function MUST be compiled in ARM mode -- THUMB will yield bogus
 * results.
 *
 * This will NOT preserve r0-r3/ip.
 */
void dvmMterpDumpArmRegs(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
  // TODO: Clang does not support asm declaration syntax.
#ifndef __clang__
    register uint32_t rPC       asm("r4");
    register uint32_t rFP       asm("r5");
    register uint32_t rSELF     asm("r6");
    register uint32_t rINST     asm("r7");
    register uint32_t rIBASE    asm("r8");
    register uint32_t r9        asm("r9");
    register uint32_t r10       asm("r10");

    //extern char dvmAsmInstructionStart[];

    printf("REGS: r0=%08x r1=%08x r2=%08x r3=%08x\n", r0, r1, r2, r3);
    printf("    : rPC=%08x rFP=%08x rSELF=%08x rINST=%08x\n",
        rPC, rFP, rSELF, rINST);
    printf("    : rIBASE=%08x r9=%08x r10=%08x\n", rIBASE, r9, r10);
#endif

    //Thread* self = (Thread*) rSELF;
    //const Method* method = self->method;
    printf("    + self is %p\n", dvmThreadSelf());
    //printf("    + currently in %s.%s %s\n",
    //    method->clazz->descriptor, method->name, method->shorty);
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
