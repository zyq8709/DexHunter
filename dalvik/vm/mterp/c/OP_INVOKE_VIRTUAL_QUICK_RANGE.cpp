HANDLE_OPCODE(OP_INVOKE_VIRTUAL_QUICK_RANGE/*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeVirtualQuick, true);
OP_END
