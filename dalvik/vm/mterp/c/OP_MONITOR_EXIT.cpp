HANDLE_OPCODE(OP_MONITOR_EXIT /*vAA*/)
    {
        Object* obj;

        EXPORT_PC();

        vsrc1 = INST_AA(inst);
        ILOGV("|monitor-exit v%d %s(0x%08x)",
            vsrc1, kSpacing+5, GET_REGISTER(vsrc1));
        obj = (Object*)GET_REGISTER(vsrc1);
        if (!checkForNull(obj)) {
            /*
             * The exception needs to be processed at the *following*
             * instruction, not the current instruction (see the Dalvik
             * spec).  Because we're jumping to an exception handler,
             * we're not actually at risk of skipping an instruction
             * by doing so.
             */
            ADJUST_PC(1);           /* monitor-exit width is 1 */
            GOTO_exceptionThrown();
        }
        ILOGV("+ unlocking %p %s", obj, obj->clazz->descriptor);
        if (!dvmUnlockObject(self, obj)) {
            assert(dvmCheckException(self));
            ADJUST_PC(1);
            GOTO_exceptionThrown();
        }
    }
    FINISH(1);
OP_END
