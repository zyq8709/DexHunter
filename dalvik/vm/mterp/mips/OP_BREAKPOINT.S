%verify "executed"
    /*
     * Breakpoint handler.
     *
     * Restart this instruction with the original opcode.  By
     * the time we get here, the breakpoint will have already been
     * handled.
     */
    move    a0, rPC
    JAL(dvmGetOriginalOpcode)           # (rPC)
    FETCH(rINST, 0)                     # reload OP_BREAKPOINT + rest of inst
    lw      a1, offThread_mainHandlerTable(rSELF)
    and     rINST, 0xff00
    or      rINST, rINST, a0
    GOTO_OPCODE_BASE(a1, a0)
