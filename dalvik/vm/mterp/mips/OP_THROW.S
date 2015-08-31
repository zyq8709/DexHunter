%verify "executed"
%verify "exception for null object"
    /*
     * Throw an exception object in the current thread.
     */
    /* throw vAA */
    GET_OPA(a2)                            #  a2 <- AA
    GET_VREG(a1, a2)                       #  a1 <- vAA (exception object)
    EXPORT_PC()                            #  exception handler can throw
    # null object?
    beqz      a1, common_errNullObject     #  yes, throw an NPE instead
    # bypass dvmSetException, just store it
    STORE_offThread_exception(a1, rSELF)   #  thread->exception <- obj
    b         common_exceptionThrown

