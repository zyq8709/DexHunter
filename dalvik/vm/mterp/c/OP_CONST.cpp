HANDLE_OPCODE(OP_CONST /*vAA, #+BBBBBBBB*/)
    {
        u4 tmp;

        vdst = INST_AA(inst);
        tmp = FETCH(1);
        tmp |= (u4)FETCH(2) << 16;
        ILOGV("|const v%d,#0x%08x", vdst, tmp);
        SET_REGISTER(vdst, tmp);
    }
    FINISH(3);
OP_END
