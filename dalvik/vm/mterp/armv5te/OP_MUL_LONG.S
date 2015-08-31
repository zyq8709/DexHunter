%verify "executed"
    /*
     * Signed 64-bit integer multiply.
     *
     * Consider WXxYZ (r1r0 x r3r2) with a long multiply:
     *        WX
     *      x YZ
     *  --------
     *     ZW ZX
     *  YW YX
     *
     * The low word of the result holds ZX, the high word holds
     * (ZW+YX) + (the high overflow from ZX).  YW doesn't matter because
     * it doesn't fit in the low 64 bits.
     *
     * Unlike most ARM math operations, multiply instructions have
     * restrictions on using the same register more than once (Rd and Rm
     * cannot be the same).
     */
    /* mul-long vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[BB]
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[CC]
    ldmia   r2, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r3, {r2-r3}                 @ r2/r3<- vCC/vCC+1
    mul     ip, r2, r1                  @  ip<- ZxW
    umull   r9, r10, r2, r0             @  r9/r10 <- ZxX
    mla     r2, r0, r3, ip              @  r2<- YxX + (ZxW)
    mov     r0, rINST, lsr #8           @ r0<- AA
    add     r10, r2, r10                @  r10<- r10 + low(ZxW + (YxX))
    add     r0, rFP, r0, lsl #2         @ r0<- &fp[AA]
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    b       .L${opcode}_finish
%break

.L${opcode}_finish:
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r0, {r9-r10}                @ vAA/vAA+1<- r9/r10
    GOTO_OPCODE(ip)                     @ jump to next instruction
