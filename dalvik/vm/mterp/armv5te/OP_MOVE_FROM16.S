%verify "executed"
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    FETCH(r1, 1)                        @ r1<- BBBB
    mov     r0, rINST, lsr #8           @ r0<- AA
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    GET_VREG(r2, r1)                    @ r2<- fp[BBBB]
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r2, r0)                    @ fp[AA]<- r2
    GOTO_OPCODE(ip)                     @ jump to next instruction
