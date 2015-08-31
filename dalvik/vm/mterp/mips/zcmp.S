%verify "branch taken"
%verify "branch not taken"
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
     * fragment that specifies the *reverse* comparison to perform, e.g.
     * for "if-le" you would use "gt".
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a2, a0)                       #  a2 <- vAA
    FETCH_S(a1, 1)                         #  a1 <- branch offset, in code units
    b${revcmp} a2, zero, 1f                #  branch to 1 if comparison failed
    b 2f
1:
    li        a1, 2                        #  a1- BYTE branch dist for not-taken
2:
    addu      a1, a1, a1                   #  convert to bytes
    FETCH_ADVANCE_INST_RB(a1)              #  update rPC, load rINST
#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    bgez      a1, 3f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh table base
3:
    bnez      a0, common_updateProfile     #  test for JIT off at target
#else
    bgez      a1, 4f
    lw        rIBASE, offThread_curHandlerTable(rSELF)  # refresh rtable base
4:
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
