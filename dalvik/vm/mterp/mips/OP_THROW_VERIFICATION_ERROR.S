%verify executed
    /*
     * Handle a throw-verification-error instruction.  This throws an
     * exception for an error discovered during verification.  The
     * exception is indicated by AA, with some detail provided by BBBB.
     */
    /* op AA, ref@BBBB */

    LOAD_rSELF_method(a0)                  #  a0 <- self->method
    FETCH(a2, 1)                           #  a2 <- BBBB
    EXPORT_PC()                            #  export the PC
    GET_OPA(a1)                            #  a1 <- AA
    JAL(dvmThrowVerificationError)         #  always throws
    b         common_exceptionThrown       #  handle exception

