%verify executed
    /*
     * Handle a throw-verification-error instruction.  This throws an
     * exception for an error discovered during verification.  The
     * exception is indicated by AA, with some detail provided by BBBB.
     */
    /* op AA, ref@BBBB */
    ldr     r0, [rSELF, #offThread_method]    @ r0<- self->method
    FETCH(r2, 1)                        @ r2<- BBBB
    EXPORT_PC()                         @ export the PC
    mov     r1, rINST, lsr #8           @ r1<- AA
    bl      dvmThrowVerificationError   @ always throws
    b       common_exceptionThrown      @ handle exception
