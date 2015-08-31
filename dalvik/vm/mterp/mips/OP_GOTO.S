%verify "executed"
%verify "forward and backward"
    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    sll       a0, rINST, 16                #  a0 <- AAxx0000
    sra       a1, a0, 24                   #  a1 <- ssssssAA (sign-extended)
    addu      a2, a1, a1                   #  a2 <- byte offset
    /* If backwards branch refresh rBASE */
    bgez      a1, 1f
    lw        rIBASE, offThread_curHandlerTable(rSELF) #  refresh handler base
1:
    FETCH_ADVANCE_INST_RB(a2)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bltz      a1, common_testUpdateProfile #  (a0) check for trace hotness
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
