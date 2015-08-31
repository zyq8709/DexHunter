HANDLE_OPCODE(OP_INVOKE_OBJECT_INIT_RANGE /*{vCCCC..v(CCCC+AA-1)}, meth@BBBB*/)
    {
        Object* obj;

        vsrc1 = FETCH(2);               /* reg number of "this" pointer */
        obj = GET_REGISTER_AS_OBJECT(vsrc1);

        if (!checkForNullExportPC(obj, fp, pc))
            GOTO_exceptionThrown();

        /*
         * The object should be marked "finalizable" when Object.<init>
         * completes normally.  We're going to assume it does complete
         * (by virtue of being nothing but a return-void) and set it now.
         */
        if (IS_CLASS_FLAG_SET(obj->clazz, CLASS_ISFINALIZABLE)) {
            EXPORT_PC();
            dvmSetFinalizable(obj);
            if (dvmGetException(self))
                GOTO_exceptionThrown();
        }

        if (self->interpBreak.ctl.subMode & kSubModeDebuggerActive) {
            /* behave like OP_INVOKE_DIRECT_RANGE */
            GOTO_invoke(invokeDirect, true);
        }
        FINISH(3);
    }
OP_END
