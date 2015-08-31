%verify "executed"
    /* const-wide/high16 vAA, #+BBBB000000000000 */
    FETCH(r1, 1)                        @ r1<- 0000BBBB (zero-extended)
    mov     r3, rINST, lsr #8           @ r3<- AA
    mov     r0, #0                      @ r0<- 00000000
    mov     r1, r1, lsl #16             @ r1<- BBBB0000
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[AA]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r3, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
