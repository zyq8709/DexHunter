HANDLE_OPCODE(OP_UNUSED_FF)
    /*
     * In portable interp, most unused opcodes will fall through to here.
     */
    ALOGE("unknown opcode 0x%02x\n", INST_INST(inst));
    dvmAbort();
    FINISH(1);
OP_END
