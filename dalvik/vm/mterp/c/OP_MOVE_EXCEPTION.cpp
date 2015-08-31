HANDLE_OPCODE(OP_MOVE_EXCEPTION /*vAA*/)
    vdst = INST_AA(inst);
    ILOGV("|move-exception v%d", vdst);
    assert(self->exception != NULL);
    SET_REGISTER(vdst, (u4)self->exception);
    dvmClearException(self);
    FINISH(1);
OP_END
