%default {"preinstr":"", "result0":"a0", "result1":"a1", "chkzero":"0", "arg0":"a0", "arg1":"a1", "arg2":"a2", "arg3":"a3"}
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64($arg0, $arg1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64($arg2, $arg3, t1)               #  a2/a3 <- vCC/vCC+1
    .if $chkzero
    or        t0, $arg2, $arg3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    $preinstr                              #  optional op
    $instr                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64($result0, $result1, rOBJ)      #  vAA/vAA+1 <- $result0/$result1
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */

