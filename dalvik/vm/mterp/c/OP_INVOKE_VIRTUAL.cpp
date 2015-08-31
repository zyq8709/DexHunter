HANDLE_OPCODE(OP_INVOKE_VIRTUAL /*vB, {vD, vE, vF, vG, vA}, meth@CCCC*/)
    GOTO_invoke(invokeVirtual, false);
OP_END
