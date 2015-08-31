%verify "executed"
%verify "null object"
    /* iget-wide-quick vA, vB, offset@CCCC */
    mov     r2, rINST, lsr #12          @ r2<- B
    FETCH(ip, 1)                        @ ip<- field byte offset
    GET_VREG(r3, r2)                    @ r3<- object we're operating on
    ubfx    r2, rINST, #8, #4           @ r2<- A
    cmp     r3, #0                      @ check object for null
    beq     common_errNullObject        @ object was null
    ldrd    r0, [r3, ip]                @ r0<- obj.field (64 bits, aligned)
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    add     r3, rFP, r2, lsl #2         @ r3<- &fp[A]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r3, {r0-r1}                 @ fp[A]<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
