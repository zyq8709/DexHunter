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
     * The operation we're implementing is:
     *   if (x == y)
     *     return 0;
     *   else if (x < y)
     *     return -1;
     *   else if (x > y)
     *     return 1;
     *   else
     *     return {-1,1};  // one or both operands was NaN
     *
     * The straightforward implementation requires 3 calls to functions
     * that return a result in r0.  We can do it with two calls if our
     * EABI library supports __aeabi_cfcmple (only one if we want to check
     * for NaN directly):
     *   check x <= y
     *     if <, return -1
     *     if ==, return 0
     *   check y <= x
     *     if <, return 1
     *   return {-1,1}
     *
     * for: cmpl-float, cmpg-float
     */
    /* op vAA, vBB, vCC */
    mov     r9, r0                      @ Save copies - we may need to redo
    mov     r10, r1
    mov     r11, lr                     @ save return address
    ldr     ip, .L__aeabi_cfcmple       @ cmp <=: C clear if <, Z set if eq
    blx     ip
    bhi     .L${opcode}_gt_or_nan       @ C set and Z clear, disambiguate
    mvncc   r0, #0                      @ (less than) r0<- -1
    moveq   r0, #0                      @ (equal) r0<- 0, trumps less than
    bx      r11
    @ Test for NaN with a second comparison.  EABI forbids testing bit
    @ patterns, and we can't represent 0x7fc00000 in immediate form, so
    @ make the library call.
.L${opcode}_gt_or_nan:
    mov     r0, r10                     @ restore in reverse order
    mov     r1, r9
    ldr     ip, .L__aeabi_cfcmple       @ r0<- Z set if eq, C clear if <
    blx     ip
    movcc   r0, #1                      @ (greater than) r1<- 1
    bxcc    r11
    $naninst                            @ r1<- 1 or -1 for NaN
    bx      r11
