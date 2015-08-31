%default { "chaintgt" : ".LinvokeChain" }
    /*
     * For polymorphic callsite, check whether the cached class pointer matches
     * the current one. If so setup the Dalvik frame and return to the
     * Thumb code through the link register to transfer control to the callee
     * method through a dedicated chaining cell.
     *
     * The predicted chaining cell is declared in ArmLIR.h with the
     * following layout:
     *
     *  typedef struct PredictedChainingCell {
     *      u4 branch;
     *      u4 delay_slot;
     *      const ClassObject *clazz;
     *      const Method *method;
     *      u4 counter;
     *  } PredictedChainingCell;
     *
     * Upon returning to the callsite:
     *    - lr   : to branch to the chaining cell
     *    - lr+8 : to punt to the interpreter
     *    - lr+16: to fully resolve the callee and may rechain.
     *             a3 <- class
     */
    # a0 = this, a1 = returnCell, a2 = predictedChainCell, rPC = dalvikCallsite
    lw      a3, offObject_clazz(a0)     # a3 <- this->class
    lw      rIBASE, 8(a2)                   # t0 <- predictedChainCell->clazz
    lw      a0, 12(a2)                  # a0 <- predictedChainCell->method
    lw      t1, offThread_icRechainCount(rSELF)    # t1 <- shared rechainCount

#if defined(WITH_JIT_TUNING)
    la      rINST, .LdvmICHitCount
    #add     t2, t2, 1
    bne    a3, rIBASE, 1f
    nop
    lw      t2, 0(rINST)
    add     t2, t2, 1
    sw      t2, 0(rINST)
1:
    #add     t2, t2, 1
#endif
    beq     a3, rIBASE, $chaintgt       # branch if predicted chain is valid
    lw      rINST, offClassObject_vtable(a3)     # rINST <- this->class->vtable
    beqz    rIBASE, 2f                      # initialized class or not
    sub     a1, t1, 1                   # count--
    sw      a1, offThread_icRechainCount(rSELF)   # write back to InterpState
    b       3f
2:
    move    a1, zero
3:
    add     ra, ra, 16                  # return to fully-resolve landing pad
    /*
     * a1 <- count
     * a2 <- &predictedChainCell
     * a3 <- this->class
     * rPC <- dPC
     * rINST <- this->class->vtable
     */
    RETURN
