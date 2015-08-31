HANDLE_OPCODE(OP_REM_DOUBLE_2ADDR /*vA, vB*/)
    vdst = INST_A(inst);
    vsrc1 = INST_B(inst);
    ILOGV("|%s-double-2addr v%d,v%d", "mod", vdst, vsrc1);
    SET_REGISTER_DOUBLE(vdst,
        fmod(GET_REGISTER_DOUBLE(vdst), GET_REGISTER_DOUBLE(vsrc1)));
    FINISH(1);
OP_END
