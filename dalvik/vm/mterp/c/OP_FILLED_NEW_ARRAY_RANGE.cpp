HANDLE_OPCODE(OP_FILLED_NEW_ARRAY_RANGE /*{vCCCC..v(CCCC+AA-1)}, class@BBBB*/)
    GOTO_invoke(filledNewArray, true);
OP_END
