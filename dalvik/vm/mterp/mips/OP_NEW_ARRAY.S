%verify "executed"
%verify "negative array length"
%verify "allocation fails"
    /*
     * Allocate an array of objects, specified with the array class
     * and a count.
     *
     * The verifier guarantees that this is an array class, so we don't
     * check for it here.
     */
    /* new-array vA, vB, class@CCCC */
    GET_OPB(a0)                            #  a0 <- B
    FETCH(a2, 1)                           #  a2 <- CCCC
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    GET_VREG(a1, a0)                       #  a1 <- vB (array length)
    LOAD_base_offDvmDex_pResClasses(a3, a3) #  a3 <- pDvmDex->pResClasses
    LOAD_eas2(a0, a3, a2)                  #  a0 <- resolved class
    # check length
    bltz      a1, common_errNegativeArraySize #  negative length, bail - len in a1
    EXPORT_PC()                            #  req'd for resolve, alloc
    # already resolved?
    beqz      a0, .L${opcode}_resolve

    /*
     * Finish allocation.
     *
     *  a0 holds class
     *  a1 holds array length
     */
.L${opcode}_finish:
    li        a2, ALLOC_DONT_TRACK         #  don't track in local refs table
    JAL(dvmAllocArrayByClass)              #  v0 <- call(clazz, length, flags)
    GET_OPA4(a2)                           #  a2 <- A+
    # failed?
    beqz      v0, common_exceptionThrown   #  yes, handle the exception
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(v0, a2)                       #  vA <- v0
    GOTO_OPCODE(t0)                        #  jump to next instruction
%break

    /*
     * Resolve class.  (This is an uncommon case.)
     *
     *  a1 holds array length
     *  a2 holds class ref CCCC
     */
.L${opcode}_resolve:
    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    move      rOBJ, a1                     #  rOBJ <- length (save)
    move      a1, a2                       #  a1 <- CCCC
    li        a2, 0                        #  a2 <- false
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    JAL(dvmResolveClass)                   #  v0 <- call(clazz, ref)
    move      a1, rOBJ                     #  a1 <- length (restore)
    # got null?
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    move      a0, v0
    b         .L${opcode}_finish           #  continue with ${opcode}_finish


