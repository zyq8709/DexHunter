%default { "naninst":"li rTEMP, -1" }
%verify "executed"
%verify "basic lt, gt, eq */
%verify "left arg NaN"
%verify "right arg NaN"
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * Provide a "naninst" instruction that puts 1 or -1 into a1 depending
     * on what value we'd like to return when one of the operands is NaN.
     *
     * The operation we're implementing is:
     *   if (x == y)
     *     return 0;
     *   else if (x < y)
     *     return -1;
     *   else if (x > y)
     *     return 1;
     *   else
     *     return {-1,1};  // one or both operands was NaN
     *
     * for: cmpl-float, cmpg-float
     */
    /* op vAA, vBB, vCC */

    /* "clasic" form */
    FETCH(a0, 1)                           #  a0 <- CCBB
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8
#ifdef SOFT_FLOAT
    GET_VREG(rOBJ, a2)                     #  rOBJ <- vBB
    GET_VREG(rBIX, a3)                     #  rBIX <- vCC
    move      a0, rOBJ                     #  a0 <- vBB
    move      a1, rBIX                     #  a1 <- vCC
    JAL(__eqsf2)                           #  a0 <- (vBB == vCC)
    li        rTEMP, 0                     # set rTEMP to 0
    beqz      v0, ${opcode}_finish
    move      a0, rOBJ                     #  a0 <- vBB
    move      a1, rBIX                     #  a1 <- vCC
    JAL(__ltsf2)                           #  a0 <- (vBB < vCC)
    li        rTEMP, -1
    bltz      v0, ${opcode}_finish
    move      a0, rOBJ                     #  a0 <- vBB
    move      a1, rBIX                     #  a1 <- vCC
    b         ${opcode}_continue
#else
    GET_VREG_F(ft0, a2)
    GET_VREG_F(ft1, a3)
    c.olt.s   fcc0, ft0, ft1               # Is ft0 < ft1
    li        rTEMP, -1
    bc1t      fcc0, ${opcode}_finish
    c.olt.s   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, ${opcode}_finish
    c.eq.s    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, ${opcode}_finish
    b         ${opcode}_nan

#endif

%break

${opcode}_nan:
    $naninst
    b         ${opcode}_finish

#ifdef SOFT_FLOAT
${opcode}_continue:
    JAL(__gtsf2)                           #  v0 <- (vBB > vCC)
    li        rTEMP, 1                     #  rTEMP = 1 if v0 != 0
    bgtz      v0, ${opcode}_finish
    b         ${opcode}_nan
#endif

${opcode}_finish:
    GET_OPA(t0)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    SET_VREG(rTEMP, t0)                    #  vAA <- rTEMP
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)
