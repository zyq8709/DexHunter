HANDLE_OPCODE(OP_CONST_WIDE /*vAA, #+BBBBBBBBBBBBBBBB*/)
    {
        u8 tmp;

        vdst = INST_AA(inst);
        tmp = FETCH(1);
        tmp |= (u8)FETCH(2) << 16;
        tmp |= (u8)FETCH(3) << 32;
        tmp |= (u8)FETCH(4) << 48;
        ILOGV("|const-wide v%d,#0x%08llx", vdst, tmp);
        SET_REGISTER_WIDE(vdst, tmp);
    }
    FINISH(5);
OP_END
