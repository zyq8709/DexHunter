%default { "isrange":"0" }
%verify "executed"
%verify "unimplemented array type"
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type@BBBB */
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResClasses]    @ r3<- pDvmDex->pResClasses
    EXPORT_PC()                         @ need for resolve and alloc
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved class
    mov     r10, rINST, lsr #8          @ r10<- AA or BA
    cmp     r0, #0                      @ already resolved?
    bne     .L${opcode}_continue        @ yes, continue on
8:  ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    mov     r2, #0                      @ r2<- false
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    bl      dvmResolveClass             @ r0<- call(clazz, ref)
    cmp     r0, #0                      @ got null?
    beq     common_exceptionThrown      @ yes, handle exception
    b       .L${opcode}_continue
%break

    /*
     * On entry:
     *  r0 holds array class
     *  r10 holds AA or BA
     */
.L${opcode}_continue:
    ldr     r3, [r0, #offClassObject_descriptor] @ r3<- arrayClass->descriptor
    mov     r2, #ALLOC_DONT_TRACK       @ r2<- alloc flags
    ldrb    rINST, [r3, #1]             @ rINST<- descriptor[1]
    .if     $isrange
    mov     r1, r10                     @ r1<- AA (length)
    .else
    mov     r1, r10, lsr #4             @ r1<- B (length)
    .endif
    cmp     rINST, #'I'                 @ array of ints?
    cmpne   rINST, #'L'                 @ array of objects?
    cmpne   rINST, #'['                 @ array of arrays?
    mov     r9, r1                      @ save length in r9
    bne     .L${opcode}_notimpl         @ no, not handled yet
    bl      dvmAllocArrayByClass        @ r0<- call(arClass, length, flags)
    cmp     r0, #0                      @ null return?
    beq     common_exceptionThrown      @ alloc failed, handle exception

    FETCH(r1, 2)                        @ r1<- FEDC or CCCC
    str     r0, [rSELF, #offThread_retval]      @ retval.l <- new array
    str     rINST, [rSELF, #offThread_retval+4] @ retval.h <- type
    add     r0, r0, #offArrayObject_contents @ r0<- newArray->contents
    subs    r9, r9, #1                  @ length--, check for neg
    FETCH_ADVANCE_INST(3)               @ advance to next instr, load rINST
    bmi     2f                          @ was zero, bail

    @ copy values from registers into the array
    @ r0=array, r1=CCCC/FEDC, r9=length (from AA or B), r10=AA/BA
    .if     $isrange
    add     r2, rFP, r1, lsl #2         @ r2<- &fp[CCCC]
1:  ldr     r3, [r2], #4                @ r3<- *r2++
    subs    r9, r9, #1                  @ count--
    str     r3, [r0], #4                @ *contents++ = vX
    bpl     1b
    @ continue at 2
    .else
    cmp     r9, #4                      @ length was initially 5?
    and     r2, r10, #15                @ r2<- A
    bne     1f                          @ <= 4 args, branch
    GET_VREG(r3, r2)                    @ r3<- vA
    sub     r9, r9, #1                  @ count--
    str     r3, [r0, #16]               @ contents[4] = vA
1:  and     r2, r1, #15                 @ r2<- F/E/D/C
    GET_VREG(r3, r2)                    @ r3<- vF/vE/vD/vC
    mov     r1, r1, lsr #4              @ r1<- next reg in low 4
    subs    r9, r9, #1                  @ count--
    str     r3, [r0], #4                @ *contents++ = vX
    bpl     1b
    @ continue at 2
    .endif

2:
    ldr     r0, [rSELF, #offThread_retval]     @ r0<- object
    ldr     r1, [rSELF, #offThread_retval+4]   @ r1<- type
    ldr     r2, [rSELF, #offThread_cardTable]  @ r2<- card table base
    GET_INST_OPCODE(ip)                      @ ip<- opcode from rINST
    cmp     r1, #'I'                         @ Is int array?
    strneb  r2, [r2, r0, lsr #GC_CARD_SHIFT] @ Mark card based on object head
    GOTO_OPCODE(ip)                          @ execute it

    /*
     * Throw an exception indicating that we have not implemented this
     * mode of filled-new-array.
     */
.L${opcode}_notimpl:
    ldr     r0, .L_strFilledNewArrayNotImpl_${opcode}
3:  add     r0, pc
    bl      dvmThrowInternalError
    b       common_exceptionThrown

    /*
     * Ideally we'd only define this once, but depending on layout we can
     * exceed the range of the load above.
     */

.L_strFilledNewArrayNotImpl_${opcode}:
    .word   PCREL_REF(.LstrFilledNewArrayNotImpl,3b)
