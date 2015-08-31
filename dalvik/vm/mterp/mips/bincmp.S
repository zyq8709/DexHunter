%verify "branch taken"
%verify "branch not taken"
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a2, a0)                       #  a2 <- vA
    b${revcmp} a2, a3, 1f                  #  branch to 1 if comparison failed
    FETCH_S(a1, 1)                         #  a1<- branch offset, in code units
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a2, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a2)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a2, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
3:
    bnez      a0, common_updateProfile
#else
    bgez      a2, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rIBASE
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
