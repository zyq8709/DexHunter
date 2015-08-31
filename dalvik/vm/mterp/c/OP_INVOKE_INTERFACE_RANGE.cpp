HANDLE_OPCODE(OP_INVOKE_INTERFACE_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeInterface, true);
OP_END
