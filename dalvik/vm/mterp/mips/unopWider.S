%default {"preinstr":"", "result0":"a0", "result1":"a1"}
    /*
     * Generic 32bit-to-64bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op a0", where
     * "result" is a 64-bit quantity in a0/a1.
     *
     * For: int-to-long, int-to-double, float-to-long, float-to-double
     */
    /* unop vA, vB */
    GET_OPA4(t1)                           #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, a3)                       #  a0 <- vB
    EAS2(rOBJ, rFP, t1)                    #  rOBJ <- &fp[A]
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $preinstr                              #  optional op
    $instr                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64($result0, $result1, rOBJ)      #  vA/vA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-11 instructions */
