HANDLE_OPCODE(OP_MONITOR_ENTER /*vAA*/)
    {
        Object* obj;

        vsrc1 = INST_AA(inst);
        ILOGV("|monitor-enter v%d %s(0x%08x)",
            vsrc1, kSpacing+6, GET_REGISTER(vsrc1));
        obj = (Object*)GET_REGISTER(vsrc1);
        if (!checkForNullExportPC(obj, fp, pc))
            GOTO_exceptionThrown();
        ILOGV("+ locking %p %s", obj, obj->clazz->descriptor);
        EXPORT_PC();    /* need for precise GC */
        dvmLockObject(self, obj);
    }
    FINISH(1);
OP_END
