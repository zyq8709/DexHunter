%verify "executed"
%verify "null object"
%verify "class cast exception thrown, with correct class name"
%verify "class cast exception not thrown on same class"
%verify "class cast exception not thrown on subclass"
%verify "class not resolved"
%verify "class already resolved"
    /*
     * Check to see if a cast from one class to another is allowed.
     */
    # check-cast vAA, class                /* BBBB */
    GET_OPA(a3)                            #  a3 <- AA
    FETCH(a2, 1)                           #  a2 <- BBBB
    GET_VREG(rOBJ, a3)                     #  rOBJ <- object
    LOAD_rSELF_methodClassDex(a0)          #  a0 <- pDvmDex
    LOAD_base_offDvmDex_pResClasses(a0, a0) #  a0 <- pDvmDex->pResClasses
    # is object null?
    beqz      rOBJ, .L${opcode}_okay       #  null obj, cast always succeeds
    LOAD_eas2(a1, a0, a2)                  #  a1 <- resolved class
    LOAD_base_offObject_clazz(a0, rOBJ)    #  a0 <- obj->clazz
    # have we resolved this before?
    beqz      a1, .L${opcode}_resolve      #  not resolved, do it now
.L${opcode}_resolved:
    # same class (trivial success)?
    bne       a0, a1, .L${opcode}_fullcheck #  no, do full check
.L${opcode}_okay:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

    /*
     * Trivial test failed, need to perform full check.  This is common.
     *  a0 holds obj->clazz
     *  a1 holds class resolved from BBBB
     *  rOBJ holds object
     */
.L${opcode}_fullcheck:
    move      rBIX,a1                      #  avoid ClassObject getting clobbered
    JAL(dvmInstanceofNonTrivial)           #  v0 <- boolean result
    # failed?
    bnez      v0, .L${opcode}_okay         #  no, success
    b         .L${opcode}_castfailure
%break

.L${opcode}_castfailure:
    # A cast has failed. We need to throw a ClassCastException with the
    # class of the object that failed to be cast.
    EXPORT_PC()                            #  about to throw
    LOAD_base_offObject_clazz(a0, rOBJ)    #  a0 <- obj->clazz
    move      a1,rBIX                      #  r1<- desired class
    JAL(dvmThrowClassCastException)
    b         common_exceptionThrown

    /*
     * Resolution required.  This is the least-likely path.
     *
     *  a2   holds BBBB
     *  rOBJ holds object
     */
.L${opcode}_resolve:
    EXPORT_PC()                            #  resolve() could throw
    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    move      a1, a2                       #  a1 <- BBBB
    li        a2, 0                        #  a2 <- false
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    JAL(dvmResolveClass)                   #  v0 <- resolved ClassObject ptr
    # got null?
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    move      a1, v0                       #  a1 <- class resolved from BBB
    LOAD_base_offObject_clazz(a0, rOBJ)    #  a0 <- obj->clazz
    b         .L${opcode}_resolved         #  pick up where we left off
