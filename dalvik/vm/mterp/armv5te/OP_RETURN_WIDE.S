%verify "executed"
    /*
     * Return a 64-bit value.  Copies the return value into the "thread"
     * structure, then jumps to the return handler.
     */
    /* return-wide vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[AA]
    add     r3, rSELF, #offThread_retval  @ r3<- &self->retval
    ldmia   r2, {r0-r1}                 @ r0/r1 <- vAA/vAA+1
    stmia   r3, {r0-r1}                 @ retval<- r0/r1
    b       common_returnFromMethod
