%verify "executed"
%verify "exception for null object"
    /*
     * Throw an exception object in the current thread.
     */
    /* throw vAA */
    EXPORT_PC
    GET_VREG_R %eax rINST              # eax<- exception object
    movl     rSELF,%ecx                # ecx<- self
    testl    %eax,%eax                 # null object?
    je       common_errNullObject
    movl     %eax,offThread_exception(%ecx) # thread->exception<- obj
    jmp      common_exceptionThrown
