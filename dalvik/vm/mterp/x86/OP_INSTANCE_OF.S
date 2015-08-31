%verify "executed"
%verify "null object"
%verify "class cast exception thrown, with correct class name"
%verify "class cast exception not thrown on same class"
%verify "class cast exception not thrown on subclass"
%verify "class not resolved"
%verify "class already resolved"
    /*
     * Check to see if an object reference is an instance of a class.
     *
     * Most common situation is a non-null object, being compared against
     * an already-resolved class.
     */
    /* instance-of vA, vB, class@CCCC */
    movl    rINST,%eax                  # eax<- BA
    sarl    $$4,%eax                    # eax<- B
    GET_VREG_R %eax %eax                # eax<- vB (obj)
    movl    rSELF,%ecx
    testl   %eax,%eax                   # object null?
    movl    offThread_methodClassDex(%ecx),%ecx  # ecx<- pDvmDex
    SPILL(rIBASE)                       # preserve rIBASE
    je      .L${opcode}_store           # null obj, not instance, store it
    movzwl  2(rPC),rIBASE               # rIBASE<- CCCC
    movl    offDvmDex_pResClasses(%ecx),%ecx # ecx<- pDvmDex->pResClasses
    movl    (%ecx,rIBASE,4),%ecx        # ecx<- resolved class
    movl    offObject_clazz(%eax),%eax  # eax<- obj->clazz
    testl   %ecx,%ecx                   # have we resolved this before?
    je      .L${opcode}_resolve         # not resolved, do it now
.L${opcode}_resolved:  # eax<- obj->clazz, ecx<- resolved class
    cmpl    %eax,%ecx                   # same class (trivial success)?
    je      .L${opcode}_trivial         # yes, trivial finish
    /*
     * Trivial test failed, need to perform full check.  This is common.
     *  eax holds obj->clazz
     *  ecx holds class resolved from BBBB
     *  rINST has BA
     */
    movl    %eax,OUT_ARG0(%esp)
    movl    %ecx,OUT_ARG1(%esp)
    call    dvmInstanceofNonTrivial     # eax<- boolean result
    # fall through to ${opcode}_store

    /*
     * eax holds boolean result
     * rINST holds BA
     */
.L${opcode}_store:
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    andb    $$0xf,rINSTbl               # <- A
    ADVANCE_PC 2
    SET_VREG %eax rINST                 # vA<- eax
    GOTO_NEXT_R %ecx

    /*
     * Trivial test succeeded, save and bail.
     *  r9 holds A
     */
.L${opcode}_trivial:
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    andb    $$0xf,rINSTbl               # <- A
    ADVANCE_PC 2
    movl    $$1,%eax
    SET_VREG %eax rINST                 # vA<- true
    GOTO_NEXT_R %ecx

    /*
     * Resolution required.  This is the least-likely path.
     *
     *  rIBASE holds BBBB
     *  rINST holds BA
     */
.L${opcode}_resolve:
    movl    rIBASE,OUT_ARG1(%esp)         # arg1<- BBBB
    movl    rSELF,%ecx
    movl    offThread_method(%ecx),%ecx
    movl    $$1,OUT_ARG2(%esp)          # arg2<- true
    movl    offMethod_clazz(%ecx),%ecx  # ecx<- method->clazz
    EXPORT_PC
    movl    %ecx,OUT_ARG0(%esp)         # arg0<- method->clazz
    call    dvmResolveClass             # eax<- resolved ClassObject ptr
    testl   %eax,%eax                   # success?
    je      common_exceptionThrown      # no, handle exception
/* Now, we need to sync up with fast path.  We need eax to
 * hold the obj->clazz, and ecx to hold the resolved class
 */
    movl    %eax,%ecx                   # ecx<- resolved class
    movl    rINST,%eax                  # eax<- BA
    sarl    $$4,%eax                    # eax<- B
    GET_VREG_R %eax %eax                # eax<- vB (obj)
    movl    offObject_clazz(%eax),%eax  # eax<- obj->clazz
    jmp     .L${opcode}_resolved
