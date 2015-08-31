%verify "executed"
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shl-long vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r3, r0, #255                @ r3<- BB
    mov     r0, r0, lsr #8              @ r0<- CC
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[BB]
    GET_VREG(r2, r0)                    @ r2<- vCC
    ldmia   r3, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]

    mov     r1, r1, asl r2              @  r1<- r1 << r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r1, r1, r0, lsr r3          @  r1<- r1 | (r0 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r1, r0, asl ip              @  if r2 >= 32, r1<- r0 << (r2-32)
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    b       .L${opcode}_finish
%break

.L${opcode}_finish:
    mov     r0, r0, asl r2              @  r0<- r0 << r2
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
