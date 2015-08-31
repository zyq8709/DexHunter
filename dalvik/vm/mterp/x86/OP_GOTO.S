%verify "executed"
%verify "forward and backward"
    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    movl    rSELF,%ecx
    movsbl  rINSTbl,%eax          # eax<- ssssssAA
    movl    offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $$0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT
