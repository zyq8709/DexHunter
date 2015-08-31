%verify "executed"
%verify "forward, backward, self"
    /*
     * Unconditional branch, 32-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     *
     * Unlike most opcodes, this one is allowed to branch to itself, so
     * our "backward branch" test must be "<=0" instead of "<0".  Because
     * we need the V bit set, we'll use an adds to convert from Dalvik
     * offset to byte offset.
     */
    /* goto/32 +AAAAAAAA */
    FETCH(r0, 1)                        @ r0<- aaaa (lo)
    FETCH(r1, 2)                        @ r1<- AAAA (hi)
    orr     r0, r0, r1, lsl #16         @ r0<- AAAAaaaa
    adds    r1, r0, r0                  @ r1<- byte offset
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrle   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
    ble     common_testUpdateProfile    @ (r0) hot trace head?
#else
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
    ldrle   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
