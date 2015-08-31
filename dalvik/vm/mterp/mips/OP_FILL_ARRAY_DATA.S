%verify "executed"
    /* fill-array-data vAA, +BBBBBBBB */
    FETCH(a0, 1)                           #  a0 <- bbbb (lo)
    FETCH(a1, 2)                           #  a1 <- BBBB (hi)
    GET_OPA(a3)                            #  a3 <- AA
    sll       a1, a1, 16                   #  a1 <- BBBBbbbb
    or        a1, a0, a1                   #  a1 <- BBBBbbbb
    GET_VREG(a0, a3)                       #  a0 <- vAA (array object)
    EAS1(a1, rPC, a1)                      #  a1 <- PC + BBBBbbbb*2 (array data off.)
    EXPORT_PC()
    JAL(dvmInterpHandleFillArrayData)      #  fill the array with predefined data
    # 0 means an exception is thrown
    beqz      v0, common_exceptionThrown   #  has exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
