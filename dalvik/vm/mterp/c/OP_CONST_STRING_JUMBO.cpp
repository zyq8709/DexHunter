HANDLE_OPCODE(OP_CONST_STRING_JUMBO /*vAA, string@BBBBBBBB*/)
    {
        StringObject* strObj;
        u4 tmp;

        vdst = INST_AA(inst);
        tmp = FETCH(1);
        tmp |= (u4)FETCH(2) << 16;
        ILOGV("|const-string/jumbo v%d string@0x%08x", vdst, tmp);
        strObj = dvmDexGetResolvedString(methodClassDex, tmp);
        if (strObj == NULL) {
            EXPORT_PC();
            strObj = dvmResolveString(curMethod->clazz, tmp);
            if (strObj == NULL)
                GOTO_exceptionThrown();
        }
        SET_REGISTER(vdst, (u4) strObj);
    }
    FINISH(3);
OP_END
