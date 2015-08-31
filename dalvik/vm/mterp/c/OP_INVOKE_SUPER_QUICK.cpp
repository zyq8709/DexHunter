HANDLE_OPCODE(OP_INVOKE_SUPER_QUICK /*vB, {vD, vE, vF, vG, vA}, meth@CCCC*/)
    GOTO_invoke(invokeSuperQuick, false);
OP_END
