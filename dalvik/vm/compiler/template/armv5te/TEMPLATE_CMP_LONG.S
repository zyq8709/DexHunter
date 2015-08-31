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
    cmp     r1, r3                      @ compare (vBB+1, vCC+1)
    blt     .L${opcode}_less            @ signed compare on high part
    bgt     .L${opcode}_greater
    subs    r0, r0, r2                  @ r0<- r0 - r2
    bxeq     lr
    bhi     .L${opcode}_greater         @ unsigned compare on low part
.L${opcode}_less:
    mvn     r0, #0                      @ r0<- -1
    bx      lr
.L${opcode}_greater:
    mov     r0, #1                      @ r0<- 1
    bx      lr
