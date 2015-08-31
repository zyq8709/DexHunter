%verify "executed"
%include "mips/unflopWide.S" {"instr":"b d2l_doconv", "st_result":"STORE64(rRESULT0, rRESULT1, rOBJ)"}
%break

d2l_doconv:
#ifdef SOFT_FLOAT
    la        t0, .LDOUBLE_TO_LONG_max
    LOAD64(rARG2, rARG3, t0)
    move      rBIX, rARG0                  #  save a0
    move      rTEMP, rARG1                 #  and a1
    JAL(__gedf2)

    move      t1, v0
    la        t0, .LDOUBLE_TO_LONG_ret_max
    LOAD64(rRESULT0, rRESULT1, t0)
    bgez      t1, .L${opcode}_set_vreg

    move      rARG0, rBIX
    move      rARG1, rTEMP
    la        t0, .LDOUBLE_TO_LONG_min
    LOAD64(rARG2, rARG3, t0)
    JAL(__ledf2)

    move      t1, v0
    la        t0, .LDOUBLE_TO_LONG_ret_min
    LOAD64(rRESULT0, rRESULT1, t0)
    blez      t1, .L${opcode}_set_vreg

    move      rARG0, rBIX
    move      rARG1, rTEMP
    move      rARG2, rBIX
    move      rARG3, rTEMP
    JAL(__nedf2)

    move      t0, v0
    li        rRESULT0, 0
    li        rRESULT1, 0
    bnez      t0, .L${opcode}_set_vreg

    move      rARG0, rBIX
    move      rARG1, rTEMP
    JAL(__fixdfdi)

#else
    la        t0, .LDOUBLE_TO_LONG_max
    LOAD64_F(fa1, fa1f, t0)
    c.ole.d   fcc0, fa1, fa0
    la        t0, .LDOUBLE_TO_LONG_ret_max
    LOAD64(rRESULT0, rRESULT1, t0)
    bc1t      .L${opcode}_set_vreg

    la        t0, .LDOUBLE_TO_LONG_min
    LOAD64_F(fa1, fa1f, t0)
    c.ole.d   fcc0, fa0, fa1
    la        t0, .LDOUBLE_TO_LONG_ret_min
    LOAD64(rRESULT0, rRESULT1, t0)
    bc1t      .L${opcode}_set_vreg

    mov.d     fa1, fa0
    c.un.d    fcc0, fa0, fa1
    li        rRESULT0, 0
    li        rRESULT1, 0
    bc1t      .L${opcode}_set_vreg
    JAL(__fixdfdi)
#endif
    b         .L${opcode}_set_vreg


.LDOUBLE_TO_LONG_max:
    .dword 0x43e0000000000000              #  maxlong, as a double (high word)
.LDOUBLE_TO_LONG_min:
    .dword 0xc3e0000000000000              #  minlong, as a double (high word)
.LDOUBLE_TO_LONG_ret_max:
    .dword 0x7fffffffffffffff
.LDOUBLE_TO_LONG_ret_min:
    .dword 0x8000000000000000
