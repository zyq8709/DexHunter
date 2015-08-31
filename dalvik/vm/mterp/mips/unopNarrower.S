%default {"preinstr":"", "load":"LOAD64_F(fa0, fa0f, a3)"}
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
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  t1 <- A+
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, a3)               #  a0/a1 <- vB/vB+1
#else
    $load
#endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $preinstr                              #  optional op
#ifdef SOFT_FLOAT
    $instr                                 #  a0 <- op, a0-a3 changed

.L${opcode}_set_vreg:
    SET_VREG(v0, rOBJ)                     #  vA <- result0
#else
    $instr_f

.L${opcode}_set_vreg_f:
    SET_VREG_F(fv0, rOBJ)                  #  vA <- result0
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-11 instructions */
