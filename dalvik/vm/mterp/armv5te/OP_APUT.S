%default { "store":"str", "shift":"2" }
%verify "executed"
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(r2, 1, 0)                   @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B(r3, 1, 1)                   @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #$shift     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r2, r9)                    @ r2<- vAA
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    $store  r2, [r0, #offArrayObject_contents]  @ vBB[vCC]<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction
