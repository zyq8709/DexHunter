%default { "naninst":"mvn     r1, #0" }
%verify "executed"
%verify "basic lt, gt, eq */
%verify "left arg NaN"
%verify "right arg NaN"
    /*
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
    FETCH(r0, 1)                        @ r0<- CCBB
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    GET_VREG(r9, r2)                    @ r9<- vBB
    GET_VREG(r10, r3)                   @ r10<- vCC
    mov     r0, r9                      @ copy to arg registers
    mov     r1, r10
    bl      __aeabi_cfcmple             @ cmp <=: C clear if <, Z set if eq
    bhi     .L${opcode}_gt_or_nan       @ C set and Z clear, disambiguate
    mvncc   r1, #0                      @ (less than) r1<- -1
    moveq   r1, #0                      @ (equal) r1<- 0, trumps less than
.L${opcode}_finish:
    mov     r3, rINST, lsr #8           @ r3<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r3)                    @ vAA<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
%break

    @ Test for NaN with a second comparison.  EABI forbids testing bit
    @ patterns, and we can't represent 0x7fc00000 in immediate form, so
    @ make the library call.
.L${opcode}_gt_or_nan:
    mov     r1, r9                      @ reverse order
    mov     r0, r10
    bl      __aeabi_cfcmple             @ r0<- Z set if eq, C clear if <
    @bleq    common_abort
    movcc   r1, #1                      @ (greater than) r1<- 1
    bcc     .L${opcode}_finish
    $naninst                            @ r1<- 1 or -1 for NaN
    b       .L${opcode}_finish


#if 0       /* "clasic" form */
    FETCH(r0, 1)                        @ r0<- CCBB
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    GET_VREG(r9, r2)                    @ r9<- vBB
    GET_VREG(r10, r3)                   @ r10<- vCC
    mov     r0, r9                      @ r0<- vBB
    mov     r1, r10                     @ r1<- vCC
    bl      __aeabi_fcmpeq              @ r0<- (vBB == vCC)
    cmp     r0, #0                      @ equal?
    movne   r1, #0                      @ yes, result is 0
    bne     ${opcode}_finish
    mov     r0, r9                      @ r0<- vBB
    mov     r1, r10                     @ r1<- vCC
    bl      __aeabi_fcmplt              @ r0<- (vBB < vCC)
    cmp     r0, #0                      @ less than?
    b       ${opcode}_continue
@%break

${opcode}_continue:
    mvnne   r1, #0                      @ yes, result is -1
    bne     ${opcode}_finish
    mov     r0, r9                      @ r0<- vBB
    mov     r1, r10                     @ r1<- vCC
    bl      __aeabi_fcmpgt              @ r0<- (vBB > vCC)
    cmp     r0, #0                      @ greater than?
    beq     ${opcode}_nan               @ no, must be NaN
    mov     r1, #1                      @ yes, result is 1
    @ fall through to _finish

${opcode}_finish:
    mov     r3, rINST, lsr #8           @ r3<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r3)                    @ vAA<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

    /*
     * This is expected to be uncommon, so we double-branch (once to here,
     * again back to _finish).
     */
${opcode}_nan:
    $naninst                            @ r1<- 1 or -1 for NaN
    b       ${opcode}_finish

#endif
