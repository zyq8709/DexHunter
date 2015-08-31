%verify "executed"
    /*
     * Breakpoint handler.
     *
     * Restart this instruction with the original opcode.  By
     * the time we get here, the breakpoint will have already been
     * handled.
     */
    mov     r0, rPC
    bl      dvmGetOriginalOpcode        @ (rPC)
    FETCH(rINST, 0)                     @ reload OP_BREAKPOINT + rest of inst
    ldr     r1, [rSELF, #offThread_mainHandlerTable]
    and     rINST, #0xff00
    orr     rINST, rINST, r0
    GOTO_OPCODE_BASE(r1, r0)
