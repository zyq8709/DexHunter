%verify "executed"
    /*
     * Return the length of an array.
     */
    GET_OPB(a1)                            #  a1 <- B
    GET_OPA4(a2)                           #  a2 <- A+
    GET_VREG(a0, a1)                       #  a0 <- vB (object ref)
    # is object null?
    beqz      a0, common_errNullObject     #  yup, fail
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- array length
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a3, a2, t0)              #  vA <- length

