HANDLE_OPCODE(OP_RETURN_VOID_BARRIER /**/)
    ILOGV("|return-void");
#ifndef NDEBUG
    retval.j = 0xababababULL;   /* placate valgrind */
#endif
    ANDROID_MEMBAR_STORE();
    GOTO_returnFromMethod();
OP_END
