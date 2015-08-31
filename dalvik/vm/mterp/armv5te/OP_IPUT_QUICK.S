%verify "executed"
%verify "null object"
    /* For: iput-quick */
    /* op vA, vB, offset@CCCC */
    mov     r2, rINST, lsr #12          @ r2<- B
    GET_VREG(r3, r2)                    @ r3<- fp[B], the object pointer
    FETCH(r1, 1)                        @ r1<- field byte offset
    cmp     r3, #0                      @ check object for null
    mov     r2, rINST, lsr #8           @ r2<- A(+)
    beq     common_errNullObject        @ object was null
    and     r2, r2, #15
    GET_VREG(r0, r2)                    @ r0<- fp[A]
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    str     r0, [r3, r1]                @ obj.field (always 32 bits)<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
