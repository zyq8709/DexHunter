HANDLE_OPCODE(OP_INSTANCE_OF /*vA, vB, class@CCCC*/)
    {
        ClassObject* clazz;
        Object* obj;

        vdst = INST_A(inst);
        vsrc1 = INST_B(inst);   /* object to check */
        ref = FETCH(1);         /* class to check against */
        ILOGV("|instance-of v%d,v%d,class@0x%04x", vdst, vsrc1, ref);

        obj = (Object*)GET_REGISTER(vsrc1);
        if (obj == NULL) {
            SET_REGISTER(vdst, 0);
        } else {
#if defined(WITH_EXTRA_OBJECT_VALIDATION)
            if (!checkForNullExportPC(obj, fp, pc))
                GOTO_exceptionThrown();
#endif
            clazz = dvmDexGetResolvedClass(methodClassDex, ref);
            if (clazz == NULL) {
                EXPORT_PC();
                clazz = dvmResolveClass(curMethod->clazz, ref, true);
                if (clazz == NULL)
                    GOTO_exceptionThrown();
            }
            SET_REGISTER(vdst, dvmInstanceof(obj->clazz, clazz));
        }
    }
    FINISH(2);
OP_END
