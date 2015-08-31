%verify "executed"
%include "mips/unflopWider.S" {"instr":"b f2l_doconv", "instr_f":"b f2l_doconv", "st_result":"STORE64(rRESULT0, rRESULT1, rOBJ)"}
%break

f2l_doconv:
#ifdef SOFT_FLOAT
    li        a1, 0x5f000000
    move      rBIX, a0
    JAL(__gesf2)

    move      t0, v0
    li        rRESULT0, ~0
    li        rRESULT1, ~0x80000000
    bgez      t0, .L${opcode}_set_vreg

    move      a0, rBIX
    li        a1, 0xdf000000
    JAL(__lesf2)

    move      t0, v0
    li        rRESULT0, 0
    li        rRESULT1, 0x80000000
    blez      t0, .L${opcode}_set_vreg

    move      a0, rBIX
    move      a1, rBIX
    JAL(__nesf2)

    move      t0, v0
    li        rRESULT0, 0
    li        rRESULT1, 0
    bnez      t0, .L${opcode}_set_vreg

    move      a0, rBIX
    JAL(__fixsfdi)

#else
    l.s       fa1, .LLONG_TO_max
    c.ole.s   fcc0, fa1, fa0
    li        rRESULT0, ~0
    li        rRESULT1, ~0x80000000
    bc1t      .L${opcode}_set_vreg

    l.s       fa1, .LLONG_TO_min
    c.ole.s   fcc0, fa0, fa1
    li        rRESULT0, 0
    li        rRESULT1, 0x80000000
    bc1t      .L${opcode}_set_vreg

    mov.s     fa1, fa0
    c.un.s    fcc0, fa0, fa1
    li        rRESULT0, 0
    li        rRESULT1, 0
    bc1t      .L${opcode}_set_vreg

    JAL(__fixsfdi)
#endif

    b         .L${opcode}_set_vreg

.LLONG_TO_max:
    .word 0x5f000000

.LLONG_TO_min:
    .word 0xdf000000
