%verify "executed"
%verify "exception for null object"
    /*
     * Synchronize on an object.
     */
    /* monitor-enter vAA */
    movl    rSELF,%ecx
    GET_VREG_R %eax rINST               # eax<- vAA
    FETCH_INST_WORD 1
    testl   %eax,%eax                   # null object?
    EXPORT_PC                           # need for precise GC
    je     common_errNullObject
    movl    %ecx,OUT_ARG0(%esp)
    movl    %eax,OUT_ARG1(%esp)
    SPILL(rIBASE)
    call    dvmLockObject               # dvmLockObject(self,object)
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx
