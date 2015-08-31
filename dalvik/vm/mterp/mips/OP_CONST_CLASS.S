%verify "executed"
%verify "Class already resolved"
%verify "Class not yet resolved"
%verify "Class cannot be resolved"
    # const/class vAA, Class               /* BBBB */
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- self->methodClassDex
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    LOAD_base_offDvmDex_pResClasses(a2, a2) #  a2 <- dvmDex->pResClasses
    LOAD_eas2(v0, a2, a1)                  #  v0 <- pResClasses[BBBB]

    bnez      v0, .L${opcode}_resolve      #  v0!=0 => resolved-ok
    /*
     * Continuation if the Class has not yet been resolved.
     *  a1: BBBB (Class ref)
     *  rOBJ: target register
     */
    EXPORT_PC()
    LOAD_rSELF_method(a0)                  #  a0 <- self->method
    li        a2, 1                        #  a2 <- true
    LOAD_base_offMethod_clazz(a0, a0)      #  a0 <- method->clazz
    JAL(dvmResolveClass)                   #  v0 <- Class reference
    # failed==0?
    beqz      v0, common_exceptionThrown   #  yup, handle the exception

.L${opcode}_resolve:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(v0, rOBJ, t0)            #  vAA <- v0


