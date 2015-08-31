HANDLE_OPCODE(OP_THROW_VERIFICATION_ERROR)
    EXPORT_PC();
    vsrc1 = INST_AA(inst);
    ref = FETCH(1);             /* class/field/method ref */
    dvmThrowVerificationError(curMethod, vsrc1, ref);
    GOTO_exceptionThrown();
OP_END
