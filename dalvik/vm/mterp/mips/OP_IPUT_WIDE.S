%default {"volatile":"0"}
%verify "executed"
%verify "null object"
%verify "field already resolved"
%verify "field not yet resolved"
%verify "field cannot be resolved"
    # iput-wide vA, vB, field              /* CCCC */
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
    # success?
    move      a0, v0
    bnez      v0, .L${opcode}_finish       #  yes, finish up
    b         common_exceptionThrown
%break

    /*
     * Currently:
     *  a0   holds resolved field
     *  rOBJ holds object
     */
.L${opcode}_finish:
    GET_OPA4(a2)                           #  a2 <- A+
    LOAD_base_offInstField_byteOffset(a3, a0) #  a3 <- byte offset of field
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[A]
    # check object for null
    beqz      rOBJ, common_errNullObject   #  object was null
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    LOAD64(a0, a1, a2)                     #  a0/a1 <- fp[A]
    GET_INST_OPCODE(rBIX)                  #  extract opcode from rINST
    addu      a2, rOBJ, a3                 #  form address
    .if $volatile
    JAL(dvmQuasiAtomicSwap64Sync)          # stores r0/r1 into addr r2
#    STORE64(a0, a1, a2)                    #  obj.field (64 bits, aligned) <- a0 a1
    .else
    STORE64(a0, a1, a2)                    #  obj.field (64 bits, aligned) <- a0 a1
    .endif
    GOTO_OPCODE(rBIX)                      #  jump to next instruction

