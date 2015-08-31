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
     * See OP_CMPL_FLOAT for an explanation.
     *
     * For: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */

    FETCH(a0, 1)                           #  a0 <- CCBB
    and       rOBJ, a0, 255                #  s0 <- BB
    srl       rBIX, a0, 8                  #  t0 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  s0 <- &fp[BB]
    EAS2(rBIX, rFP, rBIX)                  #  t0 <- &fp[CC]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vBB/vBB+1
    LOAD64(rARG2, rARG3, rBIX)             #  a2/a3 <- vCC/vCC+1
    JAL(__eqdf2)                           #  cmp <=: C clear if <, Z set if eq
    li        rTEMP, 0
    beqz      v0, ${opcode}_finish

    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vBB/vBB+1
    LOAD64(rARG2, rARG3, rBIX)             #  a2/a3 <- vCC/vCC+1
    JAL(__ltdf2)
    li        rTEMP, -1
    bltz      v0, ${opcode}_finish
    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vBB/vBB+1
    b         ${opcode}_continue
#else
    LOAD64_F(ft0, ft0f, rOBJ)
    LOAD64_F(ft1, ft1f, rBIX)
    c.olt.d   fcc0, ft0, ft1
    li        rTEMP, -1
    bc1t      fcc0, ${opcode}_finish
    c.olt.d   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, ${opcode}_finish
    c.eq.d    fcc0, ft0, ft1
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
    LOAD64(rARG2, rARG3, rBIX)             #  a2/a3 <- vCC/vCC+1
    JAL(__gtdf2)                           #  fallthru
    li        rTEMP, 1                     #  rTEMP = 1 if v0 != 0
    blez      v0, ${opcode}_nan            #  fall thru for finish
#endif

${opcode}_finish:
    GET_OPA(rOBJ)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(rTEMP, rOBJ, t0)         #  vAA <- rTEMP
