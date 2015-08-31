%verify "executed"
%verify "null object"
%verify "class cast exception thrown, with correct class name"
%verify "class cast exception not thrown on same class"
%verify "class cast exception not thrown on subclass"
%verify "class not resolved"
%verify "class already resolved"
    /*
     * Check to see if an object reference is an instance of a class.
     *
     * Most common situation is a non-null object, being compared against
     * an already-resolved class.
     */
    /* instance-of vA, vB, class@CCCC */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r3)                    @ r0<- vB (object)
    and     r9, r9, #15                 @ r9<- A
    cmp     r0, #0                      @ is object null?
    ldr     r2, [rSELF, #offThread_methodClassDex]    @ r2<- pDvmDex
    beq     .L${opcode}_store           @ null obj, not an instance, store r0
    FETCH(r3, 1)                        @ r3<- CCCC
    ldr     r2, [r2, #offDvmDex_pResClasses]    @ r2<- pDvmDex->pResClasses
    ldr     r1, [r2, r3, lsl #2]        @ r1<- resolved class
    ldr     r0, [r0, #offObject_clazz]  @ r0<- obj->clazz
    cmp     r1, #0                      @ have we resolved this before?
    beq     .L${opcode}_resolve         @ not resolved, do it now
.L${opcode}_resolved: @ r0=obj->clazz, r1=resolved class
    cmp     r0, r1                      @ same class (trivial success)?
    beq     .L${opcode}_trivial         @ yes, trivial finish
    b       .L${opcode}_fullcheck       @ no, do full check
%break

    /*
     * Trivial test failed, need to perform full check.  This is common.
     *  r0 holds obj->clazz
     *  r1 holds class resolved from BBBB
     *  r9 holds A
     */
.L${opcode}_fullcheck:
    bl      dvmInstanceofNonTrivial     @ r0<- boolean result
    @ fall through to ${opcode}_store

    /*
     * r0 holds boolean result
     * r9 holds A
     */
.L${opcode}_store:
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r9)                    @ vA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

    /*
     * Trivial test succeeded, save and bail.
     *  r9 holds A
     */
.L${opcode}_trivial:
    mov     r0, #1                      @ indicate success
    @ could b ${opcode}_store, but copying is faster and cheaper
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    SET_VREG(r0, r9)                    @ vA<- r0
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction

    /*
     * Resolution required.  This is the least-likely path.
     *
     *  r3 holds BBBB
     *  r9 holds A
     */
.L${opcode}_resolve:
    EXPORT_PC()                         @ resolve() could throw
    ldr     r0, [rSELF, #offThread_method]    @ r0<- self->method
    mov     r1, r3                      @ r1<- BBBB
    mov     r2, #1                      @ r2<- true
    ldr     r0, [r0, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveClass             @ r0<- resolved ClassObject ptr
    cmp     r0, #0                      @ got null?
    beq     common_exceptionThrown      @ yes, handle exception
    mov     r1, r0                      @ r1<- class resolved from BBB
    mov     r3, rINST, lsr #12          @ r3<- B
    GET_VREG(r0, r3)                    @ r0<- vB (object)
    ldr     r0, [r0, #offObject_clazz]  @ r0<- obj->clazz
    b       .L${opcode}_resolved        @ pick up where we left off
