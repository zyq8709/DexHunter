HANDLE_OPCODE(OP_INVOKE_VIRTUAL_QUICK /*vB, {vD, vE, vF, vG, vA}, meth@CCCC*/)
    GOTO_invoke(invokeVirtualQuick, false);
OP_END
