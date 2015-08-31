%verify "executed"
%verify "exception for null object"
    /*
     * Throw an exception object in the current thread.
     */
    /* throw vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    GET_VREG(r1, r2)                    @ r1<- vAA (exception object)
    EXPORT_PC()                         @ exception handler can throw
    cmp     r1, #0                      @ null object?
    beq     common_errNullObject        @ yes, throw an NPE instead
    @ bypass dvmSetException, just store it
    str     r1, [rSELF, #offThread_exception]  @ thread->exception<- obj
    b       common_exceptionThrown
