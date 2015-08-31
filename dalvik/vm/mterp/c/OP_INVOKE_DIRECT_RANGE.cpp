HANDLE_OPCODE(OP_INVOKE_DIRECT_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeDirect, true);
OP_END
