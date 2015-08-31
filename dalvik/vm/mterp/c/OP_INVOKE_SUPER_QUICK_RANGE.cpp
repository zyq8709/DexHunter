HANDLE_OPCODE(OP_INVOKE_SUPER_QUICK_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeSuperQuick, true);
OP_END
