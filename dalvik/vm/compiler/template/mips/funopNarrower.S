%default {"preinstr":"", "load":"LOAD64_F(fa0, fa0f, a1)"}
    /*
     * Generic 64bit-to-32bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op a0/a1", where
     * "result" is a 32-bit quantity in a0.
     *
     * For: long-to-float, double-to-int, double-to-float
     * If hard floating point support is available, use fa0 as the parameter, except for
     * long-to-float opcode.
     * (This would work for long-to-int, but that instruction is actually
     * an exact match for OP_MOVE.)
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = src dalvik register address
     *
     */
    move rINST, a0                      # save a0
#ifdef  SOFT_FLOAT
    move t0, a1                         # save a1
    LOAD64(rARG0, rARG1, t0)            # a0/a1<- vB/vB+1
    $preinstr                           # optional op
    $instr                              # v0<- op, a0-a3 changed
.L${opcode}_set_vreg:
    STORE(v0, rINST)                    # vA<- v0
#else
    $load
    $preinstr                           # optional op
    $instr_f                            # fv0 = result
.L${opcode}_set_vreg_f:
    STORE_F(fv0, rINST)                 # vA<- fv0
#endif
    RETURN
