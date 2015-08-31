%verify "branch taken"
%verify "branch not taken"
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    movzx    rINSTbl,%ecx          # ecx <- A+
    andb     $$0xf,%cl             # ecx <- A
    GET_VREG_R %eax %ecx           # eax <- vA
    sarl     $$4,rINST             # rINST<- B
    movl     rSELF,%ecx
    cmpl     (rFP,rINST,4),%eax    # compare (vA, vB)
    movl     $$2,%eax              # assume not taken
    j${revcmp}   1f
    movswl   2(rPC),%eax           # Get signed branch offset
1:
    movl     offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_INDEXED %eax
    ADVANCE_PC_INDEXED %eax
#if defined(WITH_JIT)
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $$0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT
