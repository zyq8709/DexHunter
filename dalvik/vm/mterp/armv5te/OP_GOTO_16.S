%verify "executed"
%verify "forward and backward"
    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto/16 +AAAA */
    FETCH_S(r0, 1)                      @ r0<- ssssAAAA (sign-extended)
    adds    r1, r0, r0                  @ r1<- byte offset, flags set
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    bmi     common_testUpdateProfile    @ (r0) hot trace head?
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
