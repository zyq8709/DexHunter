%verify "executed"
    /* const-wide vAA, #+HHHHhhhhBBBBbbbb */
    FETCH(r0, 1)                        @ r0<- bbbb (low)
    FETCH(r1, 2)                        @ r1<- BBBB (low middle)
    FETCH(r2, 3)                        @ r2<- hhhh (high middle)
    orr     r0, r0, r1, lsl #16         @ r0<- BBBBbbbb (low word)
    FETCH(r3, 4)                        @ r3<- HHHH (high)
    mov     r9, rINST, lsr #8           @ r9<- AA
    orr     r1, r2, r3, lsl #16         @ r1<- HHHHhhhh (high word)
    FETCH_ADVANCE_INST(5)               @ advance rPC, load rINST
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
