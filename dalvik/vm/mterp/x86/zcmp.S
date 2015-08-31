%verify "branch taken"
%verify "branch not taken"
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    cmpl     $$0,(rFP,rINST,4)     # compare (vA, 0)
    movl     rSELF,%ecx
    movl     $$2,%eax              # assume branch not taken
    j${revcmp}   1f
    movswl   2(rPC),%eax           # fetch signed displacement
    movl     offThread_curHandlerTable(%ecx),rIBASE
1:
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $$0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT
