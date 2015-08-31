%verify "executed"
%verify "null object"
    /* iput-wide-quick vA, vB, offset@CCCC */
    mov     r1, rINST, lsr #12          @ r1<- B
    ubfx    r0, rINST, #8, #4           @ r0<- A
    GET_VREG(r2, r1)                    @ r2<- fp[B], the object pointer
    add     r3, rFP, r0, lsl #2         @ r3<- &fp[A]
    cmp     r2, #0                      @ check object for null
    ldmia   r3, {r0-r1}                 @ r0/r1<- fp[A]
    beq     common_errNullObject        @ object was null
    FETCH(r3, 1)                        @ r3<- field byte offset
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    strd    r0, [r2, r3]                @ obj.field (64 bits, aligned)<- r0/r1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
