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
     * See OP_CMPL_FLOAT for an explanation.
     *
     * For: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    and     r9, r0, #255                @ r9<- BB
    mov     r10, r0, lsr #8             @ r10<- CC
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[BB]
    add     r10, rFP, r10, lsl #2       @ r10<- &fp[CC]
    ldmia   r9, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r10, {r2-r3}                @ r2/r3<- vCC/vCC+1
    bl      __aeabi_cdcmple             @ cmp <=: C clear if <, Z set if eq
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
    ldmia   r10, {r0-r1}                @ reverse order
    ldmia   r9, {r2-r3}
    bl      __aeabi_cdcmple             @ r0<- Z set if eq, C clear if <
    @bleq    common_abort
    movcc   r1, #1                      @ (greater than) r1<- 1
    bcc     .L${opcode}_finish
    $naninst                            @ r1<- 1 or -1 for NaN
    b       .L${opcode}_finish
