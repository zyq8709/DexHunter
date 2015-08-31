HANDLE_OPCODE(OP_INVOKE_STATIC /*vB, {vD, vE, vF, vG, vA}, meth@CCCC*/)
    GOTO_invoke(invokeStatic, false);
OP_END
