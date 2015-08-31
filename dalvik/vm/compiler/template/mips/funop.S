%default {"preinstr":""}
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-float, float-to-int
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = src dalvik register address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     *
     */
    move rOBJ, a0                       # save a0
#ifdef SOFT_FLOAT
    LOAD(a0, a1)                        # a0<- vBB
    $preinstr                           # optional op
    $instr                              # v0<- op, a0-a3 changed
.L${opcode}_set_vreg:
    STORE(v0, rOBJ)                     # vAA<- v0
#else
    LOAD_F(fa0, a1)                     # fa0<- vBB
    $preinstr                           # optional op
    $instr_f                            # fv0 = result
.L${opcode}_set_vreg_f:
    STORE_F(fv0, rOBJ)                  # vAA <- fv0
#endif
    RETURN
