%default { "load":"ldr", "sqnum":"0" }
%verify "executed"
%verify "null object"
%verify "field already resolved"
%verify "field not yet resolved"
%verify "field cannot be resolved"
    /*
     * General 32-bit instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- DvmDex
    FETCH(r1, 1)                        @ r1<- field ref CCCC
    ldr     r2, [r3, #offDvmDex_pResFields] @ r2<- pDvmDex->pResFields
    GET_VREG(r9, r0)                    @ r9<- fp[B], the object pointer
    ldr     r0, [r2, r1, lsl #2]        @ r0<- resolved InstField ptr
    cmp     r0, #0                      @ is resolved entry null?
    bne     .L${opcode}_finish          @ no, already resolved
8:  ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [r2, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveInstField         @ r0<- resolved InstField ptr
    cmp     r0, #0
    bne     .L${opcode}_finish
    b       common_exceptionThrown
%break

    /*
     * Currently:
     *  r0 holds resolved field
     *  r9 holds object
     */
.L${opcode}_finish:
    @bl      common_squeak${sqnum}
    cmp     r9, #0                      @ check object for null
    ldr     r3, [r0, #offInstField_byteOffset]  @ r3<- byte offset of field
    beq     common_errNullObject        @ object was null
    $load   r0, [r9, r3]                @ r0<- obj.field (8/16/32 bits)
    ubfx    r2, rINST, #8, #4           @ r2<- A
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r2)                    @ fp[A]<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
