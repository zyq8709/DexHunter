%default { "load":"lw", "barrier":"     # noop", "sqnum":"0" }
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
    # op vA, vB, field                     /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pDvmDex->pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .L${opcode}_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # test results
    move      a0, v0
    bnez      v0, .L${opcode}_finish
    b         common_exceptionThrown
%break

    /*
     * Currently:
     *  v0 holds resolved field
     *  rOBJ holds object (caller saved)
     */
.L${opcode}_finish:
    #BAL(common_squeak${sqnum})
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    addu      a3, a3, rOBJ                 #  form address
    $load a0, (a3)                         #  a0 <- obj.field (8/16/32 bits)
    $barrier                               #  acquiring load
    GET_OPA4(a2)                           #  a2 <- A+
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(a0, a2)                       #  fp[A] <- a0
    GOTO_OPCODE(t0)                        #  jump to next instruction

