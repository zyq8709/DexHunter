%default {"volatile":"0"}
%verify "executed"
%verify "null object"
%verify "field already resolved"
%verify "field not yet resolved"
%verify "field cannot be resolved"
    /*
     * Wide 32-bit instance field get.
     */
    # iget-wide vA, vB, field              /* CCCC */
    GET_OPB(a0)                            #  a0 <- B
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref CCCC
    LOAD_base_offDvmDex_pResFields(a2, a3) #  a2 <- pResFields
    GET_VREG(rOBJ, a0)                     #  rOBJ <- fp[B], the object pointer
    LOAD_eas2(a0, a2, a1)                  #  a0 <- resolved InstField ptr
    # is resolved entry null?
    bnez      a0, .L${opcode}_finish       #  no, already resolved
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    EXPORT_PC()                            #  resolve() could throw
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveInstField)               #  v0 <- resolved InstField ptr
    # test return code
    move      a0, v0
    bnez      v0, .L${opcode}_finish
    b         common_exceptionThrown
%break

    /*
     * Currently:
     *  a0   holds resolved field
     *  rOBJ holds object
     */
.L${opcode}_finish:
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    beqz      rOBJ, common_errNullObject   #  object was null
    GET_OPA4(a2)                           #  a2 <- A+
    addu      rOBJ, rOBJ, a3               #  form address
    .if $volatile
    vLOAD64(a0, a1, rOBJ)                  #  a0/a1 <- obj.field (64-bit align ok)
    .else
    LOAD64(a0, a1, rOBJ)                   #  a0/a1 <- obj.field (64-bit align ok)
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    EAS2(a3, rFP, a2)                      #  a3 <- &fp[A]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a3)                    #  fp[A] <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction

