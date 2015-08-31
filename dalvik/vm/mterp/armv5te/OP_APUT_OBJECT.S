%verify "executed"
    /*
     * Store an object into an array.  vBB[vCC] <- vAA.
     */
    /* op vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    GET_VREG(rINST, r2)                 @ rINST<- vBB (array object)
    GET_VREG(r1, r3)                    @ r1<- vCC (requested index)
    cmp     rINST, #0                   @ null array object?
    GET_VREG(r9, r9)                    @ r9<- vAA
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [rINST, #offArrayObject_length]   @ r3<- arrayObj->length
    add     r10, rINST, r1, lsl #2      @ r10<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcc     .L${opcode}_finish          @ we're okay, continue on
    b       common_errArrayIndex        @ index >= length, bail

%break
    /*
     * On entry:
     *  rINST = vBB (arrayObj)
     *  r9 = vAA (obj)
     *  r10 = offset into array (vBB + vCC * width)
     */
.L${opcode}_finish:
    cmp     r9, #0                      @ storing null reference?
    beq     .L${opcode}_skip_check      @ yes, skip type checks
    ldr     r0, [r9, #offObject_clazz]  @ r0<- obj->clazz
    ldr     r1, [rINST, #offObject_clazz]  @ r1<- arrayObj->clazz
    bl      dvmCanPutArrayElement       @ test object type vs. array type
    cmp     r0, #0                      @ okay?
    beq     .L${opcode}_throw           @ no
    mov     r1, rINST                   @ r1<- arrayObj
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    ldr     r2, [rSELF, #offThread_cardTable]     @ get biased CT base
    add     r10, #offArrayObject_contents   @ r0<- pointer to slot
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    str     r9, [r10]                   @ vBB[vCC]<- vAA
    strb    r2, [r2, r1, lsr #GC_CARD_SHIFT] @ mark card using object head
    GOTO_OPCODE(ip)                     @ jump to next instruction
.L${opcode}_skip_check:
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    str     r9, [r10, #offArrayObject_contents] @ vBB[vCC]<- vAA
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
.L${opcode}_throw:
    @ The types don't match.  We need to throw an ArrayStoreException.
    ldr     r0, [r9, #offObject_clazz]
    ldr     r1, [rINST, #offObject_clazz]
    EXPORT_PC()
    bl      dvmThrowArrayStoreExceptionIncompatibleElement
    b       common_exceptionThrown
