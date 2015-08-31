%verify "executed"
%include "mips/unflop.S" {"instr":"b f2i_doconv", "instr_f":"b f2i_doconv"}
%break

/*
 * Not an entry point as it is used only once !!
 */
f2i_doconv:
#ifdef SOFT_FLOAT
    li        a1, 0x4f000000               #  (float)maxint
    move      rBIX, a0
    JAL(__gesf2)                           #  is arg >= maxint?
    move      t0, v0
    li        v0, ~0x80000000              #  return maxint (7fffffff)
    bgez      t0, .L${opcode}_set_vreg

    move      a0, rBIX                     #  recover arg
    li        a1, 0xcf000000               #  (float)minint
    JAL(__lesf2)

    move      t0, v0
    li        v0, 0x80000000               #  return minint (80000000)
    blez      t0, .L${opcode}_set_vreg
    move      a0, rBIX
    move      a1, rBIX
    JAL(__nesf2)

    move      t0, v0
    li        v0, 0                        #  return zero for NaN
    bnez      t0, .L${opcode}_set_vreg

    move      a0, rBIX
    JAL(__fixsfsi)
    b         .L${opcode}_set_vreg
#else
    l.s       fa1, .LFLOAT_TO_INT_max
    c.ole.s   fcc0, fa1, fa0
    l.s       fv0, .LFLOAT_TO_INT_ret_max
    bc1t      .L${opcode}_set_vreg_f

    l.s       fa1, .LFLOAT_TO_INT_min
    c.ole.s   fcc0, fa0, fa1
    l.s       fv0, .LFLOAT_TO_INT_ret_min
    bc1t      .L${opcode}_set_vreg_f

    mov.s     fa1, fa0
    c.un.s    fcc0, fa0, fa1
    li.s      fv0, 0
    bc1t      .L${opcode}_set_vreg_f

    trunc.w.s  fv0, fa0
    b         .L${opcode}_set_vreg_f
#endif

.LFLOAT_TO_INT_max:
    .word 0x4f000000
.LFLOAT_TO_INT_min:
    .word 0xcf000000
.LFLOAT_TO_INT_ret_max:
    .word 0x7fffffff
.LFLOAT_TO_INT_ret_min:
    .word 0x80000000

