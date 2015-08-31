HANDLE_OPCODE(OP_PACKED_SWITCH /*vAA, +BBBB*/)
    {
        const u2* switchData;
        u4 testVal;
        s4 offset;

        vsrc1 = INST_AA(inst);
        offset = FETCH(1) | (((s4) FETCH(2)) << 16);
        ILOGV("|packed-switch v%d +0x%04x", vsrc1, offset);
        switchData = pc + offset;       // offset in 16-bit units
#ifndef NDEBUG
        if (switchData < curMethod->insns ||
            switchData >= curMethod->insns + dvmGetMethodInsnsSize(curMethod))
        {
            /* should have been caught in verifier */
            EXPORT_PC();
            dvmThrowInternalError("bad packed switch");
            GOTO_exceptionThrown();
        }
#endif
        testVal = GET_REGISTER(vsrc1);

        offset = dvmInterpHandlePackedSwitch(switchData, testVal);
        ILOGV("> branch taken (0x%04x)", offset);
        if (offset <= 0)  /* uncommon */
            PERIODIC_CHECKS(offset);
        FINISH(offset);
    }
OP_END
