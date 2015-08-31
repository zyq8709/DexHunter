%verify "executed"
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    mov     r1, rINST, lsr #12          @ r1<- B from 15:12
    ubfx    r0, rINST, #8, #4           @ r0<- A from 11:8
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    GET_VREG(r2, r1)                    @ r2<- fp[B]
    GET_INST_OPCODE(ip)                 @ ip<- opcode from rINST
    SET_VREG(r2, r0)                    @ fp[A]<- r2
    GOTO_OPCODE(ip)                     @ execute next instruction
