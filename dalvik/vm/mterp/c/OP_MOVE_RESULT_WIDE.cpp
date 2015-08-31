HANDLE_OPCODE(OP_MOVE_RESULT_WIDE /*vAA*/)
    vdst = INST_AA(inst);
    ILOGV("|move-result-wide v%d %s(0x%08llx)", vdst, kSpacing, retval.j);
    SET_REGISTER_WIDE(vdst, retval.j);
    FINISH(1);
OP_END
