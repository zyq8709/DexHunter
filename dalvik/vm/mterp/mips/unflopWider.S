%default {"preinstr":"", "st_result":"STORE64_F(fv0, fv0f, rOBJ)"}
    /*
     * Generic 32bit-to-64bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op a0", where
     * "result" is a 64-bit quantity in a0/a1.
     *
     * For: int-to-double, float-to-long, float-to-double
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
#ifdef SOFT_FLOAT
    GET_VREG(a0, a3)                       #  a0 <- vB
#else
    GET_VREG_F(fa0, a3)
#endif
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $preinstr                              #  optional op
#ifdef SOFT_FLOAT
    $instr                                 #  result <- op, a0-a3 changed

.L${opcode}_set_vreg:
    STORE64(rRESULT0, rRESULT1, rOBJ)      #  vA/vA+1 <- a0/a1
#else
    $instr_f

.L${opcode}_set_vreg:
    $st_result                             #  vA/vA+1 <- a0/a1
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-11 instructions */
