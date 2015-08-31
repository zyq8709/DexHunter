HANDLE_OPCODE(OP_CONST_CLASS /*vAA, class@BBBB*/)
    {
        ClassObject* clazz;

        vdst = INST_AA(inst);
        ref = FETCH(1);
        ILOGV("|const-class v%d class@0x%04x", vdst, ref);
        clazz = dvmDexGetResolvedClass(methodClassDex, ref);
        if (clazz == NULL) {
            EXPORT_PC();
            clazz = dvmResolveClass(curMethod->clazz, ref, true);
            if (clazz == NULL)
                GOTO_exceptionThrown();
        }
        SET_REGISTER(vdst, (u4) clazz);
    }
    FINISH(2);
OP_END
