%default {"preinstr":"", "st_result":"STORE64_F(fv0, fv0f, rOBJ)"}
    /*
     * Generic 32bit-to-64bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "d0 = op s0".
     *
     * For: int-to-double, float-to-double
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = src dalvik register address
     */
    /* unop vA, vB */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    LOAD(a0, a1)                        # a0<- vB
    $preinstr                           # optional op
    $instr                              # result<- op, a0-a3 changed

.L${opcode}_set_vreg:
    STORE64(rRESULT0, rRESULT1, rOBJ)   # vA/vA+1<- v0/v1
#else
    LOAD_F(fa0, a1)                     # fa0<- vB
    $preinstr                           # optional op
    $instr_f

.L${opcode}_set_vreg:
    $st_result                          # vA/vA+1<- fv0/fv0f
#endif
    RETURN
