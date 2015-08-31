%verify "executed"
%verify "null object"
%verify "class cast exception thrown, with correct class name"
%verify "class cast exception not thrown on same class"
%verify "class cast exception not thrown on subclass"
%verify "class not resolved"
%verify "class already resolved"
    /*
     * Check to see if an object reference is an instance of a class.
     *
     * Most common situation is a non-null object, being compared against
     * an already-resolved class.
     */
    # instance-of vA, vB, class            /* CCCC */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB (object)
    LOAD_rSELF_methodClassDex(a2)          #  a2 <- pDvmDex
    # is object null?
    beqz      a0, .L${opcode}_store        #  null obj, not an instance, store a0
    FETCH(a3, 1)                           #  a3 <- CCCC
    LOAD_base_offDvmDex_pResClasses(a2, a2) #  a2 <- pDvmDex->pResClasses
    LOAD_eas2(a1, a2, a3)                  #  a1 <- resolved class
    LOAD_base_offObject_clazz(a0, a0)      #  a0 <- obj->clazz
    # have we resolved this before?
    beqz      a1, .L${opcode}_resolve      #  not resolved, do it now
.L${opcode}_resolved:                   #  a0=obj->clazz, a1=resolved class
    # same class (trivial success)?
    beq       a0, a1, .L${opcode}_trivial  #  yes, trivial finish
    b         .L${opcode}_fullcheck        #  no, do full check

    /*
     * Trivial test succeeded, save and bail.
     *  rOBJ holds A
     */
.L${opcode}_trivial:
    li        a0, 1                        #  indicate success
    # fall thru
    /*
     * a0   holds boolean result
     * rOBJ holds A
     */
.L${opcode}_store:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    SET_VREG(a0, rOBJ)                     #  vA <- a0
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

%break

    /*
     * Trivial test failed, need to perform full check.  This is common.
     *  a0   holds obj->clazz
     *  a1   holds class resolved from BBBB
     *  rOBJ holds A
     */
.L${opcode}_fullcheck:
    JAL(dvmInstanceofNonTrivial)           #  v0 <- boolean result
    move      a0, v0                       #  fall through to ${opcode}_store
    b         .L${opcode}_store

    /*
     * Resolution required.  This is the least-likely path.
     *
     *  a3   holds BBBB
     *  rOBJ holds A
     */
.L${opcode}_resolve:
    EXPORT_PC()                            #  resolve() could throw
    LOAD_rSELF_method(a0)                  #  a0 <- self->method
    move      a1, a3                       #  a1 <- BBBB
    li        a2, 1                        #  a2 <- true
    LOAD_base_offMethod_clazz(a0, a0)      #  a0 <- method->clazz
    JAL(dvmResolveClass)                   #  v0 <- resolved ClassObject ptr
    # got null?
    move      a1, v0                       #  a1 <- class resolved from BBB
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, a3)                       #  a0 <- vB (object)
    LOAD_base_offObject_clazz(a0, a0)      #  a0 <- obj->clazz
    b         .L${opcode}_resolved         #  pick up where we left off

