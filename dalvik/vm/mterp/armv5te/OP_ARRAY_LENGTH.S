%verify "executed"
    /*
     * Return the length of an array.
     */
    mov     r1, rINST, lsr #12          @ r1<- B
    mov     r2, rINST, lsr #8           @ r2<- A+
    GET_VREG(r0, r1)                    @ r0<- vB (object ref)
    and     r2, r2, #15                 @ r2<- A
    cmp     r0, #0                      @ is object null?
    beq     common_errNullObject        @ yup, fail
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- array length
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r3, r2)                    @ vB<- length
    GOTO_OPCODE(ip)                     @ jump to next instruction
