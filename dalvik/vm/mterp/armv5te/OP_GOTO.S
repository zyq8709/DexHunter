%verify "executed"
%verify "forward and backward"
    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    /* tuning: use sbfx for 6t2+ targets */
    mov     r0, rINST, lsl #16          @ r0<- AAxx0000
    movs    r1, r0, asr #24             @ r1<- ssssssAA (sign-extended)
    add     r2, r1, r1                  @ r2<- byte offset, set flags
       @ If backwards branch refresh rIBASE
    ldrmi   rIBASE, [rSELF, #offThread_curHandlerTable] @ refresh handler base
    FETCH_ADVANCE_INST_RB(r2)           @ update rPC, load rINST
#if defined(WITH_JIT)
    ldr     r0, [rSELF, #offThread_pJitProfTable]
    bmi     common_testUpdateProfile    @ (r0) check for trace hotness
#endif
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
