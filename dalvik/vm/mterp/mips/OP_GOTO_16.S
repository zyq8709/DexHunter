%verify "executed"
%verify "forward and backward"
    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto/16 +AAAA */
    FETCH_S(a0, 1)                         #  a0 <- ssssAAAA (sign-extended)
    addu      a1, a0, a0                   #  a1 <- byte offset, flags set
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
    bgez      a1, 1f
    lw        rIBASE, offThread_curHandlerTable(rSELF) #  refresh handler base
1:
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bltz      a1, common_testUpdateProfile #  (a0) hot trace head?
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
