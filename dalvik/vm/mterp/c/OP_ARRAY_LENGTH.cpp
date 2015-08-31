HANDLE_OPCODE(OP_ARRAY_LENGTH /*vA, vB*/)
    {
        ArrayObject* arrayObj;

        vdst = INST_A(inst);
        vsrc1 = INST_B(inst);
        arrayObj = (ArrayObject*) GET_REGISTER(vsrc1);
        ILOGV("|array-length v%d,v%d  (%p)", vdst, vsrc1, arrayObj);
        if (!checkForNullExportPC((Object*) arrayObj, fp, pc))
            GOTO_exceptionThrown();
        /* verifier guarantees this is an array reference */
        SET_REGISTER(vdst, arrayObj->length);
    }
    FINISH(1);
OP_END
