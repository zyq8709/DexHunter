%verify "executed"
%verify "exception for null object (impossible in javac)"
%verify "dvmUnlockObject fails"
    /*
     * Unlock an object.
     *
     * Exceptions that occur when unlocking a monitor need to appear as
     * if they happened at the following instruction.  See the Dalvik
     * instruction spec.
     */
    /* monitor-exit vAA */
    GET_VREG_R %eax rINST
    movl    rSELF,%ecx
    EXPORT_PC
    testl   %eax,%eax                   # null object?
    je      .L${opcode}_errNullObject   # go if so
    movl    %eax,OUT_ARG1(%esp)
    movl    %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call    dvmUnlockObject             # unlock(self,obj)
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 1 %ecx
    testl   %eax,%eax                   # success?
    ADVANCE_PC 1
    je      common_exceptionThrown      # no, exception pending
    GOTO_NEXT_R %ecx
.L${opcode}_errNullObject:
    ADVANCE_PC 1                        # advance before throw
    jmp     common_errNullObject
