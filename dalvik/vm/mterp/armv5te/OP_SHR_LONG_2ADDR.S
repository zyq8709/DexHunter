%verify "executed"
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shr-long/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r2, r3)                    @ r2<- vB
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1

    mov     r0, r0, lsr r2              @  r0<- r2 >> r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @  r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    movpl   r0, r1, asr ip              @  if r2 >= 32, r0<-r1 >> (r2-32)
    mov     r1, r1, asr r2              @  r1<- r1 >> r2
    b       .L${opcode}_finish
%break

.L${opcode}_finish:
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
