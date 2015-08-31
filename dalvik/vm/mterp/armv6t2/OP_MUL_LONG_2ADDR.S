%verify "executed"
    /*
     * Signed 64-bit integer multiply, "/2addr" version.
     *
     * See OP_MUL_LONG for an explanation.
     *
     * We get a little tight on registers, so to avoid looking up &fp[A]
     * again we stuff it into rINST.
     */
    /* mul-long/2addr vA, vB */
    mov     r1, rINST, lsr #12          @ r1<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    add     r1, rFP, r1, lsl #2         @ r1<- &fp[B]
    add     rINST, rFP, r9, lsl #2      @ rINST<- &fp[A]
    ldmia   r1, {r2-r3}                 @ r2/r3<- vBB/vBB+1
    ldmia   rINST, {r0-r1}              @ r0/r1<- vAA/vAA+1
    mul     ip, r2, r1                  @  ip<- ZxW
    umull   r9, r10, r2, r0             @  r9/r10 <- ZxX
    mla     r2, r0, r3, ip              @  r2<- YxX + (ZxW)
    mov     r0, rINST                   @ r0<- &fp[A] (free up rINST)
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    add     r10, r2, r10                @  r10<- r10 + low(ZxW + (YxX))
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r0, {r9-r10}                @ vAA/vAA+1<- r9/r10
    GOTO_OPCODE(ip)                     @ jump to next instruction
