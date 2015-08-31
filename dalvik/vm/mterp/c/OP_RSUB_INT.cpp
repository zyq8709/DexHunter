HANDLE_OPCODE(OP_RSUB_INT /*vA, vB, #+CCCC*/)
    {
        vdst = INST_A(inst);
        vsrc1 = INST_B(inst);
        vsrc2 = FETCH(1);
        ILOGV("|rsub-int v%d,v%d,#+0x%04x", vdst, vsrc1, vsrc2);
        SET_REGISTER(vdst, (s2) vsrc2 - (s4) GET_REGISTER(vsrc1));
    }
    FINISH(2);
OP_END
