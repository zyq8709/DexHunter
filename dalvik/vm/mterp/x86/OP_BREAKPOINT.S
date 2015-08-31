%verify "executed"
    /*
     * Breakpoint handler.
     *
     * Restart this instruction with the original opcode.  By
     * the time we get here, the breakpoint will have already been
     * handled.  We also assume that all other special "checkBefore"
     * actions have been handled, so we'll transition directly
     * to the real handler
     */
    SPILL(rIBASE)
    movl    rPC,OUT_ARG0(%esp)
    call    dvmGetOriginalOpcode
    UNSPILL(rIBASE)
    movl    rSELF,%ecx
    movzbl  1(rPC),rINST
    movl    offThread_mainHandlerTable(%ecx),%ecx
    jmp     *(%ecx,%eax,4)

