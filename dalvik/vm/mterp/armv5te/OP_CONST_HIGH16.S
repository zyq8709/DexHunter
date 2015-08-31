%verify "executed"
    /* const/high16 vAA, #+BBBB0000 */
    FETCH(r0, 1)                        @ r0<- 0000BBBB (zero-extended)
    mov     r3, rINST, lsr #8           @ r3<- AA
    mov     r0, r0, lsl #16             @ r0<- BBBB0000
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r3)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
