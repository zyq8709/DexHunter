%default {"preinstr":""}
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-float, float-to-int
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  t0 <- A+
#ifdef SOFT_FLOAT
    GET_VREG(a0, a3)                       #  a0 <- vB
#else
    GET_VREG_F(fa0, a3)
#endif
    $preinstr                              #  optional op
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
#ifdef SOFT_FLOAT
    $instr                                 #  a0 <- op, a0-a3 changed

.L${opcode}_set_vreg:
    SET_VREG(v0, rOBJ)                     #  vAA <- result0
#else
    $instr_f

.L${opcode}_set_vreg_f:
    SET_VREG_F(fv0, rOBJ)
#endif
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    GOTO_OPCODE(t1)                        #  jump to next instruction
    /* 9-10 instructions */
