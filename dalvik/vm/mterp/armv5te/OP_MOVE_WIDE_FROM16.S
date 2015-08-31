%verify "executed"
    /* move-wide/from16 vAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    FETCH(r3, 1)                        @ r3<- BBBB
    mov     r2, rINST, lsr #8           @ r2<- AA
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[BBBB]
    add     r2, rFP, r2, lsl #2         @ r2<- &fp[AA]
    ldmia   r3, {r0-r1}                 @ r0/r1<- fp[BBBB]
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r2, {r0-r1}                 @ fp[AA]<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
