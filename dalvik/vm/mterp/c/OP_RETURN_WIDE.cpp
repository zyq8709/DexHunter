HANDLE_OPCODE(OP_RETURN_WIDE /*vAA*/)
    vsrc1 = INST_AA(inst);
    ILOGV("|return-wide v%d", vsrc1);
    retval.j = GET_REGISTER_WIDE(vsrc1);
    GOTO_returnFromMethod();
OP_END
