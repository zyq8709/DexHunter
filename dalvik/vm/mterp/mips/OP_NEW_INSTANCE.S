%verify "executed"
%verify "class not resolved"
%verify "class cannot be resolved"
%verify "class not initialized"
%verify "class fails to initialize"
%verify "class already resolved/initialized"
%verify "class is abstract or interface"
%verify "allocation fails"
    /*
     * Create a new instance of a class.
     */
    # new-instance vAA, class              /* BBBB */
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResClasses(a3, a3) #  a3 <- pDvmDex->pResClasses
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved class
#if defined(WITH_JIT)
    EAS2(rBIX, a3, a1)                     #  rBIX <- &resolved_class
#endif
    EXPORT_PC()                            #  req'd for init, resolve, alloc
    # already resolved?
    beqz      a0, .L${opcode}_resolve      #  no, resolve it now
.L${opcode}_resolved:                      #  a0=class
    lbu       a1, offClassObject_status(a0) #  a1 <- ClassStatus enum
    # has class been initialized?
    li        t0, CLASS_INITIALIZED
    move      rOBJ, a0                     #  save a0
    bne       a1, t0, .L${opcode}_needinit #  no, init class now

.L${opcode}_initialized:                   #  a0=class
    LOAD_base_offClassObject_accessFlags(a3, a0) #  a3 <- clazz->accessFlags
    li        a1, ALLOC_DONT_TRACK         #  flags for alloc call
    # a0=class
    JAL(dvmAllocObject)                    #  v0 <- new object
    GET_OPA(a3)                            #  a3 <- AA
#if defined(WITH_JIT)
    /*
     * The JIT needs the class to be fully resolved before it can
     * include this instruction in a trace.
     */
    lhu       a1, offThread_subMode(rSELF)
    beqz      v0, common_exceptionThrown   #  yes, handle the exception
    and       a1, kSubModeJitTraceBuild    #  under construction?
    bnez      a1, .L${opcode}_jitCheck
#else
    # failed?
    beqz      v0, common_exceptionThrown   #  yes, handle the exception
#endif
    b         .L${opcode}_continue

%break

.L${opcode}_continue:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(v0, a3)                       #  vAA <- v0
    GOTO_OPCODE(t0)                        #  jump to next instruction

#if defined(WITH_JIT)
    /*
     * Check to see if we need to stop the trace building early.
     * v0: new object
     * a3: vAA
     */
.L${opcode}_jitCheck:
    lw        a1, 0(rBIX)                  #  reload resolved class
    # okay?
    bnez      a1, .L${opcode}_continue     #  yes, finish
    move      rOBJ, v0                     #  preserve new object
    move      rBIX, a3                     #  preserve vAA
    move      a0, rSELF
    move      a1, rPC
    JAL(dvmJitEndTraceSelect)              #  (self, pc)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(rOBJ, rBIX)                   #  vAA <- new object
    GOTO_OPCODE(t0)                        #  jump to next instruction
#endif

    /*
     * Class initialization required.
     *
     *  a0 holds class object
     */
.L${opcode}_needinit:
    JAL(dvmInitClass)                      #  initialize class
    move      a0, rOBJ                     #  restore a0
    # check boolean result
    bnez      v0, .L${opcode}_initialized  #  success, continue
    b         common_exceptionThrown       #  failed, deal with init exception


    /*
     * Resolution required.  This is the least-likely path.
     *
     *  a1 holds BBBB
     */
.L${opcode}_resolve:
    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    li        a2, 0                        #  a2 <- false
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    JAL(dvmResolveClass)                   #  v0 <- resolved ClassObject ptr
    move      a0, v0
    # got null?
    bnez      v0, .L${opcode}_resolved     #  no, continue
    b         common_exceptionThrown       #  yes, handle exception
