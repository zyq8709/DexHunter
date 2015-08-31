%verify "executed"
    /* fill-array-data vAA, +BBBBBBBB */
    FETCH(r0, 1)                        @ r0<- bbbb (lo)
    FETCH(r1, 2)                        @ r1<- BBBB (hi)
    mov     r3, rINST, lsr #8           @ r3<- AA
    orr     r1, r0, r1, lsl #16         @ r1<- BBBBbbbb
    GET_VREG(r0, r3)                    @ r0<- vAA (array object)
    add     r1, rPC, r1, lsl #1         @ r1<- PC + BBBBbbbb*2 (array data off.)
    EXPORT_PC();
    bl      dvmInterpHandleFillArrayData@ fill the array with predefined data
    cmp     r0, #0                      @ 0 means an exception is thrown
    beq     common_exceptionThrown      @ has exception
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
