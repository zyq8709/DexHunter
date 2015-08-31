%verify "executed"
    /*
     * Array put, 64 bits.  vBB[vCC] <- vAA.
     *
     * Arrays of long/double are 64-bit aligned, so it's okay to use STRD.
     */
    /* aput-wide vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    GET_VREG(r0, r2)                    @ r0<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #offArrayObject_length]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #3          @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[AA]
    bcc     .L${opcode}_finish          @ okay, continue below
    b       common_errArrayIndex        @ index >= length, bail
    @ May want to swap the order of these two branches depending on how the
    @ branch prediction (if any) handles conditional forward branches vs.
    @ unconditional forward branches.
%break

.L${opcode}_finish:
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldmia   r9, {r2-r3}                 @ r2/r3<- vAA/vAA+1
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    strd    r2, [r0, #offArrayObject_contents]  @ r2/r3<- vBB[vCC]
    GOTO_OPCODE(ip)                     @ jump to next instruction
