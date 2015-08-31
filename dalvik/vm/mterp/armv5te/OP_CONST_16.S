%verify "executed"
    /* const/16 vAA, #+BBBB */
    FETCH_S(r0, 1)                      @ r0<- ssssBBBB (sign-extended)
    mov     r3, rINST, lsr #8           @ r3<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r3)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
