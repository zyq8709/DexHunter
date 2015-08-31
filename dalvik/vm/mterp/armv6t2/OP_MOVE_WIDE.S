%verify "executed"
    /* move-wide vA, vB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    r2, rINST, #8, #4           @ r2<- A
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[B]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[A]
    ldmia   r3, {r0-r1}                 @ r0/r1<- fp[B]
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r2, {r0-r1}                 @ fp[A]<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
