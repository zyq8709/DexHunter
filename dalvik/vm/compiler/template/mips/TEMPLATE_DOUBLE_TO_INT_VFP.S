%verify "executed"
%include "mips/funopNarrower.S" {"instr":"b    d2i_doconv","instr_f":"b    d2i_doconv"}

/*
 * Convert the double in a0/a1 to an int in a0.
 *
 * We have to clip values to int min/max per the specification.  The
 * expected common case is a "reasonable" value that converts directly
 * to modest integer.  The EABI convert function isn't doing this for us.
 * Use rBIX / rOBJ as global to hold arguments (they are not bound to a global var)
 */

d2i_doconv:
#ifdef SOFT_FLOAT
    la          t0, .LDOUBLE_TO_INT_max
    LOAD64(rARG2, rARG3, t0)
    move        rBIX, rARG0                       # save a0
    move        rOBJ, rARG1                       #  and a1
    JAL(__gedf2)                               # is arg >= maxint?

    move        t0, v0
    li          v0, ~0x80000000                # return maxint (7fffffff)
    bgez        t0, .L${opcode}_set_vreg       # nonzero == yes

    move        rARG0, rBIX                       # recover arg
    move        rARG1, rOBJ
    la          t0, .LDOUBLE_TO_INT_min
    LOAD64(rARG2, rARG3, t0)
    JAL(__ledf2)                               # is arg <= minint?

    move        t0, v0
    li          v0, 0x80000000                 # return minint (80000000)
    blez        t0, .L${opcode}_set_vreg       # nonzero == yes

    move        rARG0, rBIX                  # recover arg
    move        rARG1, rOBJ
    move        rARG2, rBIX                  # compare against self
    move        rARG3, rOBJ
    JAL(__nedf2)                        # is arg == self?

    move        t0, v0                  # zero == no
    li          v0, 0
    bnez        t0, .L${opcode}_set_vreg        # return zero for NaN

    move        rARG0, rBIX                  # recover arg
    move        rARG1, rOBJ
    JAL(__fixdfsi)                      # convert double to int
    b           .L${opcode}_set_vreg
#else
    la          t0, .LDOUBLE_TO_INT_max
    LOAD64_F(fa1, fa1f, t0)
    c.ole.d     fcc0, fa1, fa0
    l.s         fv0, .LDOUBLE_TO_INT_maxret
    bc1t        .L${opcode}_set_vreg_f

    la          t0, .LDOUBLE_TO_INT_min
    LOAD64_F(fa1, fa1f, t0)
    c.ole.d     fcc0, fa0, fa1
    l.s         fv0, .LDOUBLE_TO_INT_minret
    bc1t        .L${opcode}_set_vreg_f

    mov.d       fa1, fa0
    c.un.d      fcc0, fa0, fa1
    li.s        fv0, 0
    bc1t        .L${opcode}_set_vreg_f

    trunc.w.d   fv0, fa0
    b           .L${opcode}_set_vreg_f
#endif


.LDOUBLE_TO_INT_max:
    .dword   0x41dfffffffc00000
.LDOUBLE_TO_INT_min:
    .dword   0xc1e0000000000000                  # minint, as a double (high word)
.LDOUBLE_TO_INT_maxret:
    .word   0x7fffffff
.LDOUBLE_TO_INT_minret:
    .word   0x80000000
