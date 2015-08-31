%verify "executed"
%verify "forward, backward, self"
    /*
     * Unconditional branch, 32-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     *
     * Unlike most opcodes, this one is allowed to branch to itself, so
     * our "backward branch" test must be "<=0" instead of "<0".
     */
    /* goto/32 +AAAAAAAA */
    FETCH(a0, 1)                           #  a0 <- aaaa (lo)
    FETCH(a1, 2)                           #  a1 <- AAAA (hi)
    sll       a1, a1, 16
    or        a0, a0, a1                   #  a0 <- AAAAaaaa
    addu      a1, a0, a0                   #  a1 <- byte offset
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgtz      a1, 1f
    lw        rIBASE, offThread_curHandlerTable(rSELF) #  refresh handler base
1:
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
    blez      a1, common_testUpdateProfile # (a0) hot trace head?
#else
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
    bgtz      a0, 2f
    lw        rIBASE, offThread_curHandlerTable(rSELF) #  refresh handler base
2:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
