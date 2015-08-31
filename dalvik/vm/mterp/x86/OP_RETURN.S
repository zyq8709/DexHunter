%verify "executed"
    /*
     * Return a 32-bit value.  Copies the return value into the "self"
     * structure, then jumps to the return handler.
     *
     * for: return, return-object
     */
    /* op vAA */
    movl    rSELF,%ecx
    GET_VREG_R %eax rINST               # eax<- vAA
    movl    %eax,offThread_retval(%ecx)   # retval.i <- AA
    jmp     common_returnFromMethod
