%verify "executed"
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    FETCH(r1, 2)                        @ r1<- BBBB
    FETCH(r0, 1)                        @ r0<- AAAA
    FETCH_ADVANCE_INST(3)               @ advance rPC, load rINST
    GET_VREG(r2, r1)                    @ r2<- fp[BBBB]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r2, r0)                    @ fp[AAAA]<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction
