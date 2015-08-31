%verify "executed"
    /* const vAA, #+BBBBbbbb */
    mov     r3, rINST, lsr #8           @ r3<- AA
    FETCH(r0, 1)                        @ r0<- bbbb (low)
    FETCH(r1, 2)                        @ r1<- BBBB (high)
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    orr     r0, r0, r1, lsl #16         @ r0<- BBBBbbbb
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r3)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
