%verify "executed"
/* EABI appears to have Java-style conversions of +inf/-inf/NaN */
%include "armv5te/unop.S" {"instr":"bl      __aeabi_f2iz"}

#if 0
@include "armv5te/unop.S" {"instr":"bl      f2i_doconv"}
@break
/*
 * Convert the float in r0 to an int in r0.
 *
 * We have to clip values to int min/max per the specification.  The
 * expected common case is a "reasonable" value that converts directly
 * to modest integer.  The EABI convert function isn't doing this for us.
 */
f2i_doconv:
    stmfd   sp!, {r4, lr}
    mov     r1, #0x4f000000             @ (float)maxint
    mov     r4, r0
    bl      __aeabi_fcmpge              @ is arg >= maxint?
    cmp     r0, #0                      @ nonzero == yes
    mvnne   r0, #0x80000000             @ return maxint (7fffffff)
    ldmnefd sp!, {r4, pc}

    mov     r0, r4                      @ recover arg
    mov     r1, #0xcf000000             @ (float)minint
    bl      __aeabi_fcmple              @ is arg <= minint?
    cmp     r0, #0                      @ nonzero == yes
    movne   r0, #0x80000000             @ return minint (80000000)
    ldmnefd sp!, {r4, pc}

    mov     r0, r4                      @ recover arg
    mov     r1, r4
    bl      __aeabi_fcmpeq              @ is arg == self?
    cmp     r0, #0                      @ zero == no
    ldmeqfd sp!, {r4, pc}               @ return zero for NaN

    mov     r0, r4                      @ recover arg
    bl      __aeabi_f2iz                @ convert float to int
    ldmfd   sp!, {r4, pc}
#endif
