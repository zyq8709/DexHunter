%verify "executed"
    /*
     * Array put, 64 bits.  vBB[vCC] <- vAA.
     *
     * Arrays of long/double are 64-bit aligned, so it's okay to use STRD.
     */
    /* aput-wide vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t0)                            #  t0 <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    EAS3(a0, a0, a1)                       #  a0 <- arrayObj + index*width
    EAS2(rOBJ, rFP, t0)                    #  rOBJ <- &fp[AA]
    # compare unsigned index, length
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    LOAD64(a2, a3, rOBJ)                   #  a2/a3 <- vAA/vAA+1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64_off(a2, a3, a0, offArrayObject_contents) #  a2/a3 <- vBB[vCC]
    GOTO_OPCODE(t0)                        #  jump to next instruction

