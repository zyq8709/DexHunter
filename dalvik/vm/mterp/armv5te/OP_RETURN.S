%verify "executed"
    /*
     * Return a 32-bit value.  Copies the return value into the "thread"
     * structure, then jumps to the return handler.
     *
     * for: return, return-object
     */
    /* op vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    GET_VREG(r0, r2)                    @ r0<- vAA
    str     r0, [rSELF, #offThread_retval] @ retval.i <- vAA
    b       common_returnFromMethod
