HANDLE_OPCODE(OP_CONST_WIDE_16 /*vAA, #+BBBB*/)
    vdst = INST_AA(inst);
    vsrc1 = FETCH(1);
    ILOGV("|const-wide/16 v%d,#0x%04x", vdst, (s2)vsrc1);
    SET_REGISTER_WIDE(vdst, (s2)vsrc1);
    FINISH(2);
OP_END
