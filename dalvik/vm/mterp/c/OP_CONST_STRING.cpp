HANDLE_OPCODE(OP_CONST_STRING /*vAA, string@BBBB*/)
    {
        StringObject* strObj;

        vdst = INST_AA(inst);
        ref = FETCH(1);
        ILOGV("|const-string v%d string@0x%04x", vdst, ref);
        strObj = dvmDexGetResolvedString(methodClassDex, ref);
        if (strObj == NULL) {
            EXPORT_PC();
            strObj = dvmResolveString(curMethod->clazz, ref);
            if (strObj == NULL)
                GOTO_exceptionThrown();
        }
        SET_REGISTER(vdst, (u4) strObj);
    }
    FINISH(2);
OP_END
