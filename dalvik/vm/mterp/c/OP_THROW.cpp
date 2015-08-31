HANDLE_OPCODE(OP_THROW /*vAA*/)
    {
        Object* obj;

        /*
         * We don't create an exception here, but the process of searching
         * for a catch block can do class lookups and throw exceptions.
         * We need to update the saved PC.
         */
        EXPORT_PC();

        vsrc1 = INST_AA(inst);
        ILOGV("|throw v%d  (%p)", vsrc1, (void*)GET_REGISTER(vsrc1));
        obj = (Object*) GET_REGISTER(vsrc1);
        if (!checkForNull(obj)) {
            /* will throw a null pointer exception */
            LOGVV("Bad exception");
        } else {
            /* use the requested exception */
            dvmSetException(self, obj);
        }
        GOTO_exceptionThrown();
    }
OP_END
