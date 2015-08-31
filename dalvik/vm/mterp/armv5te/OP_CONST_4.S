%verify "executed"
    /* const/4 vA, #+B */
    mov     r1, rINST, lsl #16          @ r1<- Bxxx0000
    mov     r0, rINST, lsr #8           @ r0<- A+
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    mov     r1, r1, asr #28             @ r1<- sssssssB (sign-extended)
    and     r0, r0, #15
    GET_INST_OPCODE(ip)                 @ ip<- opcode from rINST
    SET_VREG(r1, r0)                    @ fp[A]<- r1
    GOTO_OPCODE(ip)                     @ execute next instruction
