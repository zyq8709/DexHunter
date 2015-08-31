%verify "executed"
    /*
     * Return a 32-bit value.  Copies the return value into the "thread"
     * structure, then jumps to the return handler.
     *
     * for: return, return-object
     */
    /* op vAA */
    GET_OPA(a2)                            #  a2 <- AA
    GET_VREG(a0, a2)                       #  a0 <- vAA
    sw        a0, offThread_retval(rSELF)  #  retval.i <- vAA
    b         common_returnFromMethod

