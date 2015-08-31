%default {"volatile":"0"}
%verify "executed"
%verify "field already resolved"
%verify "field not yet resolved"
%verify "field cannot be resolved"
    /*
     * 64-bit SGET handler.
     */
    # sget-wide vAA, field                 /* BBBB */
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- DvmDex
    FETCH(a1, 1)                           #  a1 <- field ref BBBB
    LOAD_base_offDvmDex_pResFields(rBIX, a2) #  rBIX <- dvmDex->pResFields
    LOAD_eas2(a0, rBIX, a1)                #  a0 <- resolved StaticField ptr
    # is resolved entry null?
    bnez      a0, .L${opcode}_finish

    /*
     * Continuation if the field has not yet been resolved.
     *  a1:  BBBB field ref
     *  rBIX: dvmDex->pResFields
     *
     * Returns StaticField pointer in v0.
     */
    LOAD_rSELF_method(a2)                  #  a2 <- current method
#if defined(WITH_JIT)
    EAS2(rBIX, rBIX, a1)                   #  rBIX<- &dvmDex->pResFields[field]
#endif
    EXPORT_PC()                            #  resolve() could throw, so export now
    LOAD_base_offMethod_clazz(a0, a2)      #  a0 <- method->clazz
    JAL(dvmResolveStaticField)             #  v0 <- resolved StaticField ptr
    move      a0, v0
    # success?
    beqz      v0, common_exceptionThrown   #  no, handle exception
#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including this instruction.
     */
    JAL(common_verifyField)
#endif

    b        .L${opcode}_finish            # resume
%break

.L${opcode}_finish:
    GET_OPA(a1)                            #  a1 <- AA
    .if $volatile
    vLOAD64_off(a2, a3, a0, offStaticField_value) #  a2/a3 <- field value (aligned)
    .else
    LOAD64_off(a2, a3, a0, offStaticField_value) #  a2/a3 <- field value (aligned)
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[AA]
    STORE64(a2, a3, a1)                    #  vAA/vAA+1 <- a2/a3
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


