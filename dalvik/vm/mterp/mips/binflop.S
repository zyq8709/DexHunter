%default {"preinstr":"", "chkzero":"0"}
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */

    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  s5 <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
#ifdef SOFT_FLOAT
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if $chkzero
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
#else
    GET_VREG_F(fa1, a3)                    #  a1 <- vCC
    GET_VREG_F(fa0, a2)                    #  a0 <- vBB

    .if $chkzero
    # is second operand zero?
    li.s      ft0, 0
    c.eq.s    fcc0, ft0, fa1               #  condition bit and comparision with 0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    $preinstr                              #  optional op
#ifdef SOFT_FLOAT
    $instr                                 #  v0 = result
    SET_VREG(v0, rOBJ)                     #  vAA <- v0
#else
    $instr_f                               #  f0 = result
    SET_VREG_F(fv0, rOBJ)                  #  vAA <- fv0
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 11-14 instructions */

