%verify "branch taken"
%verify "branch not taken"
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    mov     r1, rINST, lsr #12          @ r1<- B
    ubfx    r0, rINST, #8, #4           @ r0<- A
    GET_VREG(r3, r1)                    @ r3<- vB
    GET_VREG(r2, r0)                    @ r2<- vA
    FETCH_S(r1, 1)                      @ r1<- branch offset, in code units
    cmp     r2, r3                      @ compare (vA, vB)
    mov${revcmp} r1, #2                 @ r1<- BYTE branch dist for not-taken
    adds    r2, r1, r1                  @ convert to bytes, check sign
    FETCH_ADVANCE_INST_RB(r2)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
    cmp     r0, #0
    bne     common_updateProfile
#else
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable]  @ refresh rIBASE
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
