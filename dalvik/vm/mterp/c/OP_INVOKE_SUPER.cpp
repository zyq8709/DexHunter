HANDLE_OPCODE(OP_INVOKE_SUPER /*vB, {vD, vE, vF, vG, vA}, meth@CCCC*/)
    GOTO_invoke(invokeSuper, false);
OP_END
