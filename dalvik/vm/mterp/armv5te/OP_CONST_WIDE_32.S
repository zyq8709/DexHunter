%verify "executed"
    /* const-wide/32 vAA, #+BBBBbbbb */
    FETCH(r0, 1)                        @ r0<- 0000bbbb (low)
    mov     r3, rINST, lsr #8           @ r3<- AA
    FETCH_S(r2, 2)                      @ r2<- ssssBBBB (high)
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    orr     r0, r0, r2, lsl #16         @ r0<- BBBBbbbb
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[AA]
    mov     r1, r0, asr #31             @ r1<- ssssssss
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r3, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
