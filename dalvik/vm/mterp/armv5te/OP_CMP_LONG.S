%verify "executed"
%verify "basic lt, gt, eq"
%verify "hi equal, lo <=>"
%verify "lo equal, hi <=>"
    /*
     * Compare two 64-bit values.  Puts 0, 1, or -1 into the destination
     * register based on the results of the comparison.
     *
     * We load the full values with LDM, but in practice many values could
     * be resolved by only looking at the high word.  This could be made
     * faster or slower by splitting the LDM into a pair of LDRs.
     *
     * If we just wanted to set condition flags, we could do this:
     *  subs    ip, r0, r2
     *  sbcs    ip, r1, r3
     *  subeqs  ip, r0, r2
     * Leaving { <0, 0, >0 } in ip.  However, we have to set it to a specific
     * integer value, which we can do with 2 conditional mov/mvn instructions
     * (set 1, set -1; if they're equal we already have 0 in ip), giving
     * us a constant 5-cycle path plus a branch at the end to the
     * instruction epilogue code.  The multi-compare approach below needs
     * 2 or 3 cycles + branch if the high word doesn't match, 6 + branch
     * in the worst case (the 64-bit values are equal).
     */
    /* cmp-long vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[BB]
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[CC]
    ldmia   r2, {r0-r1}                 @ r0/r1<- vBB/vBB+1
    ldmia   r3, {r2-r3}                 @ r2/r3<- vCC/vCC+1
    cmp     r1, r3                      @ compare (vBB+1, vCC+1)
    blt     .L${opcode}_less            @ signed compare on high part
    bgt     .L${opcode}_greater
    subs    r1, r0, r2                  @ r1<- r0 - r2
    bhi     .L${opcode}_greater         @ unsigned compare on low part
    bne     .L${opcode}_less
    b       .L${opcode}_finish          @ equal; r1 already holds 0
%break

.L${opcode}_less:
    mvn     r1, #0                      @ r1<- -1
    @ Want to cond code the next mov so we can avoid branch, but don't see it;
    @ instead, we just replicate the tail end.
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r9)                    @ vAA<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

.L${opcode}_greater:
    mov     r1, #1                      @ r1<- 1
    @ fall through to _finish

.L${opcode}_finish:
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r1, r9)                    @ vAA<- r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
