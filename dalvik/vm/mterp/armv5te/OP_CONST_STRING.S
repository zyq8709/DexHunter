%verify "executed"
%verify "String already resolved"
%verify "String not yet resolved"
%verify "String cannot be resolved"
    /* const/string vAA, String@BBBB */
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r2, [rSELF, #offThread_methodClassDex]  @ r2<- self->methodClassDex
    mov     r9, rINST, lsr #8           @ r9<- AA
    ldr     r2, [r2, #offDvmDex_pResStrings]   @ r2<- dvmDex->pResStrings
    ldr     r0, [r2, r1, lsl #2]        @ r0<- pResStrings[BBBB]
    cmp     r0, #0                      @ not yet resolved?
    beq     .L${opcode}_resolve
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
%break

    /*
     * Continuation if the String has not yet been resolved.
     *  r1: BBBB (String ref)
     *  r9: target register
     */
.L${opcode}_resolve:
    EXPORT_PC()
    ldr     r0, [rSELF, #offThread_method] @ r0<- self->method
    ldr     r0, [r0, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveString            @ r0<- String reference
    cmp     r0, #0                      @ failed?
    beq     common_exceptionThrown      @ yup, handle the exception
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
