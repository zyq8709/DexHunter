%verify "executed"
%verify "String already resolved"
%verify "String not yet resolved"
%verify "String cannot be resolved"
    # const/string vAA, String             /* BBBBBBBB */
    FETCH(a0, 1)                           #  a0 <- bbbb (low)
    FETCH(a1, 2)                           #  a1 <- BBBB (high)
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- self->methodClassDex
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    LOAD_base_offDvmDex_pResStrings(a2, a2) #  a2 <- dvmDex->pResStrings
    sll       a1, a1, 16
    or        a1, a1, a0                   #  a1 <- BBBBbbbb
    LOAD_eas2(v0, a2, a1)                  #  v0 <- pResStrings[BBBB]
    bnez      v0, .L${opcode}_resolve

    /*
     * Continuation if the String has not yet been resolved.
     *  a1: BBBBBBBB (String ref)
     *  rOBJ: target register
     */
    EXPORT_PC()
    LOAD_rSELF_method(a0)                  #  a0 <- self->method
    LOAD_base_offMethod_clazz(a0, a0)      #  a0 <- method->clazz
    JAL(dvmResolveString)                  #  v0 <- String reference
    # failed?
    beqz      v0, common_exceptionThrown   #  yup, handle the exception

.L${opcode}_resolve:
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(v0, rOBJ, t1)            #  vAA <- v0

