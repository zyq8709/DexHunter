%default {"result":"%eax"}
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = eax op (rFP,%ecx,4)".
     * This could be an x86 instruction or a function call.  (If the result
     * comes back in a register other than eax, you can override "result".)
     *
     * For: add-int, sub-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax   # eax<- BB
    movzbl   3(rPC),%ecx   # ecx<- CC
    GET_VREG_R %eax %eax   # eax<- vBB
    $instr                 # ex: addl    (rFP,%ecx,4),%eax
    SET_VREG $result rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
