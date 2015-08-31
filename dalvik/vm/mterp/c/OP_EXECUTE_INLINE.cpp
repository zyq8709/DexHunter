HANDLE_OPCODE(OP_EXECUTE_INLINE /*vB, {vD, vE, vF, vG}, inline@CCCC*/)
    {
        /*
         * This has the same form as other method calls, but we ignore
         * the 5th argument (vA).  This is chiefly because the first four
         * arguments to a function on ARM are in registers.
         *
         * We only set the arguments that are actually used, leaving
         * the rest uninitialized.  We're assuming that, if the method
         * needs them, they'll be specified in the call.
         *
         * However, this annoys gcc when optimizations are enabled,
         * causing a "may be used uninitialized" warning.  Quieting
         * the warnings incurs a slight penalty (5%: 373ns vs. 393ns
         * on empty method).  Note that valgrind is perfectly happy
         * either way as the uninitialiezd values are never actually
         * used.
         */
        u4 arg0, arg1, arg2, arg3;
        arg0 = arg1 = arg2 = arg3 = 0;

        EXPORT_PC();

        vsrc1 = INST_B(inst);       /* #of args */
        ref = FETCH(1);             /* inline call "ref" */
        vdst = FETCH(2);            /* 0-4 register indices */
        ILOGV("|execute-inline args=%d @%d {regs=0x%04x}",
            vsrc1, ref, vdst);

        assert((vdst >> 16) == 0);  // 16-bit type -or- high 16 bits clear
        assert(vsrc1 <= 4);

        switch (vsrc1) {
        case 4:
            arg3 = GET_REGISTER(vdst >> 12);
            /* fall through */
        case 3:
            arg2 = GET_REGISTER((vdst & 0x0f00) >> 8);
            /* fall through */
        case 2:
            arg1 = GET_REGISTER((vdst & 0x00f0) >> 4);
            /* fall through */
        case 1:
            arg0 = GET_REGISTER(vdst & 0x0f);
            /* fall through */
        default:        // case 0
            ;
        }

        if (self->interpBreak.ctl.subMode & kSubModeDebugProfile) {
            if (!dvmPerformInlineOp4Dbg(arg0, arg1, arg2, arg3, &retval, ref))
                GOTO_exceptionThrown();
        } else {
            if (!dvmPerformInlineOp4Std(arg0, arg1, arg2, arg3, &retval, ref))
                GOTO_exceptionThrown();
        }
    }
    FINISH(3);
OP_END
