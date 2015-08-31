HANDLE_OPCODE(OP_RETURN_VOID /**/)
    ILOGV("|return-void");
#ifndef NDEBUG
    retval.j = 0xababababULL;    // placate valgrind
#endif
    GOTO_returnFromMethod();
OP_END
