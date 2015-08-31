HANDLE_OPCODE(OP_CONST_WIDE_32 /*vAA, #+BBBBBBBB*/)
    {
        u4 tmp;

        vdst = INST_AA(inst);
        tmp = FETCH(1);
        tmp |= (u4)FETCH(2) << 16;
        ILOGV("|const-wide/32 v%d,#0x%08x", vdst, tmp);
        SET_REGISTER_WIDE(vdst, (s4) tmp);
    }
    FINISH(3);
OP_END
