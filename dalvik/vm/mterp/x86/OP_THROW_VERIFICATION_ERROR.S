%verify executed
    /*
     * Handle a throw-verification-error instruction.  This throws an
     * exception for an error discovered during verification.  The
     * exception is indicated by AA, with some detail provided by BBBB.
     */
    /* op AA, ref@BBBB */
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                     # eax<- BBBB
    movl     offThread_method(%ecx),%ecx       # ecx<- self->method
    EXPORT_PC
    movl     %eax,OUT_ARG2(%esp)             # arg2<- BBBB
    movl     rINST,OUT_ARG1(%esp)            # arg1<- AA
    movl     %ecx,OUT_ARG0(%esp)             # arg0<- method
    call     dvmThrowVerificationError       # call(method, kind, ref)
    jmp      common_exceptionThrown          # handle exception
