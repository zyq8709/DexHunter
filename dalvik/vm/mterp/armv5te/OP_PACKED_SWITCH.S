%default { "func":"dvmInterpHandlePackedSwitch" }
%verify executed
    /*
     * Handle a packed-switch or sparse-switch instruction.  In both cases
     * we decode it and hand it off to a helper function.
     *
     * We don't really expect backward branches in a switch statement, but
     * they're perfectly legal, so we check for them here.
     *
     * When the JIT is present, all targets are considered treated as
     * a potential trace heads regardless of branch direction.
     *
     * for: packed-switch, sparse-switch
     */
    /* op vAA, +BBBB */
    FETCH(r0, 1)                        @ r0<- bbbb (lo)
    FETCH(r1, 2)                        @ r1<- BBBB (hi)
    mov     r3, rINST, lsr #8           @ r3<- AA
    orr     r0, r0, r1, lsl #16         @ r0<- BBBBbbbb
    GET_VREG(r1, r3)                    @ r1<- vAA
    add     r0, rPC, r0, lsl #1         @ r0<- PC + BBBBbbbb*2
    bl      $func                       @ r0<- code-unit branch offset
    adds    r1, r0, r0                  @ r1<- byte offset; clear V
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrle   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
    cmp     r0, #0
    bne     common_updateProfile
#else
    ldrle   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
    FETCH_ADVANCE_INST_RB(r1)           @ update rPC, load rINST
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
