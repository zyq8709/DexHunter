HANDLE_OPCODE(OP_INVOKE_VIRTUAL_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeVirtual, true);
OP_END
