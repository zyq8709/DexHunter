%default { "naninst":"mvn     r0, #0" }
    /*
     * For the JIT: incoming arguments in r0-r1, r2-r3
     *              result in r0
     *
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * Provide a "naninst" instruction that puts 1 or -1 into r1 depending
     * on what value we'd like to return when one of the operands is NaN.
     *
     * See OP_CMPL_FLOAT for an explanation.
     *
     * For: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */
    push    {r0-r3}                     @ save operands
    mov     r11, lr                     @ save return address
    ldr     ip, .L__aeabi_cdcmple       @ PIC way of "bl __aeabi_cdcmple"
    blx     ip
    bhi     .L${opcode}_gt_or_nan       @ C set and Z clear, disambiguate
    mvncc   r0, #0                      @ (less than) r1<- -1
    moveq   r0, #0                      @ (equal) r1<- 0, trumps less than
    add     sp, #16                     @ drop unused operands
    bx      r11

    @ Test for NaN with a second comparison.  EABI forbids testing bit
    @ patterns, and we can't represent 0x7fc00000 in immediate form, so
    @ make the library call.
.L${opcode}_gt_or_nan:
    pop     {r2-r3}                     @ restore operands in reverse order
    pop     {r0-r1}                     @ restore operands in reverse order
    ldr     ip, .L__aeabi_cdcmple       @ r0<- Z set if eq, C clear if <
    blx     ip
    movcc   r0, #1                      @ (greater than) r1<- 1
    bxcc    r11
    $naninst                            @ r1<- 1 or -1 for NaN
    bx      r11
