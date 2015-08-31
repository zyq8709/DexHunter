%verify "executed"
%verify "negative array length"
%verify "allocation fails"
    /*
     * Allocate an array of objects, specified with the array class
     * and a count.
     *
     * The verifier guarantees that this is an array class, so we don't
     * check for it here.
     */
    /* new-array vA, vB, class@CCCC */
    mov     r0, rINST, lsr #12          @ r0<- B
    FETCH(r2, 1)                        @ r2<- CCCC
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    GET_VREG(r1, r0)                    @ r1<- vB (array length)
    ldr     r3, [r3, #offDvmDex_pResClasses]    @ r3<- pDvmDex->pResClasses
    cmp     r1, #0                      @ check length
    ldr     r0, [r3, r2, lsl #2]        @ r0<- resolved class
    bmi     common_errNegativeArraySize @ negative length, bail - len in r1
    cmp     r0, #0                      @ already resolved?
    EXPORT_PC()                         @ req'd for resolve, alloc
    bne     .L${opcode}_finish          @ resolved, continue
    b       .L${opcode}_resolve         @ do resolve now
%break


    /*
     * Resolve class.  (This is an uncommon case.)
     *
     *  r1 holds array length
     *  r2 holds class ref CCCC
     */
.L${opcode}_resolve:
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    mov     r9, r1                      @ r9<- length (save)
    mov     r1, r2                      @ r1<- CCCC
    mov     r2, #0                      @ r2<- false
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveClass             @ r0<- call(clazz, ref)
    cmp     r0, #0                      @ got null?
    mov     r1, r9                      @ r1<- length (restore)
    beq     common_exceptionThrown      @ yes, handle exception
    @ fall through to ${opcode}_finish

    /*
     * Finish allocation.
     *
     *  r0 holds class
     *  r1 holds array length
     */
.L${opcode}_finish:
    mov     r2, #ALLOC_DONT_TRACK       @ don't track in local refs table
    bl      dvmAllocArrayByClass        @ r0<- call(clazz, length, flags)
    cmp     r0, #0                      @ failed?
    mov     r2, rINST, lsr #8           @ r2<- A+
    beq     common_exceptionThrown      @ yes, handle the exception
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    and     r2, r2, #15                 @ r2<- A
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ vA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
