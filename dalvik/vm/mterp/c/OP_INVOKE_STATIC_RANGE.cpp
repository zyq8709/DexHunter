HANDLE_OPCODE(OP_INVOKE_STATIC_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    GOTO_invoke(invokeStatic, true);
OP_END
