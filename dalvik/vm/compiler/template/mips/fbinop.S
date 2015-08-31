%default {"preinstr":"", "chkzero":"0"}
    /*
     * Generic 32-bit binary float operation. a0 = a1 op a2.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = op1 address
     *     a2 = op2 address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     *
     */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    LOAD(a0, a1)                        # a0<- vBB
    LOAD(a1, a2)                        # a1<- vCC
    .if $chkzero
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    $preinstr                           # optional op
    $instr                              # v0 = result
    STORE(v0, rOBJ)                     # vAA <- v0
#else
    LOAD_F(fa0, a1)                     # fa0<- vBB
    LOAD_F(fa1, a2)                     # fa1<- vCC
    .if $chkzero
    # is second operand zero?
    li.s        ft0, 0
    c.eq.s      fcc0, ft0, fa1          # condition bit and comparision with 0
    bc1t        fcc0, common_errDivideByZero
    .endif
    $preinstr                           # optional op
    $instr_f                            # fv0 = result
    STORE_F(fv0, rOBJ)                  # vAA <- fv0
#endif
    RETURN
