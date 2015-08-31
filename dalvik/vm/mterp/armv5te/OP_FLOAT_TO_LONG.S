%verify "executed"
@include "armv5te/unopWider.S" {"instr":"bl      __aeabi_f2lz"}
%include "armv5te/unopWider.S" {"instr":"bl      f2l_doconv"}

%break
/*
 * Convert the float in r0 to a long in r0/r1.
 *
 * We have to clip values to long min/max per the specification.  The
 * expected common case is a "reasonable" value that converts directly
 * to modest integer.  The EABI convert function isn't doing this for us.
 */
f2l_doconv:
    stmfd   sp!, {r4, lr}
    mov     r1, #0x5f000000             @ (float)maxlong
    mov     r4, r0
    bl      __aeabi_fcmpge              @ is arg >= maxlong?
    cmp     r0, #0                      @ nonzero == yes
    mvnne   r0, #0                      @ return maxlong (7fffffff)
    mvnne   r1, #0x80000000
    ldmnefd sp!, {r4, pc}

    mov     r0, r4                      @ recover arg
    mov     r1, #0xdf000000             @ (float)minlong
    bl      __aeabi_fcmple              @ is arg <= minlong?
    cmp     r0, #0                      @ nonzero == yes
    movne   r0, #0                      @ return minlong (80000000)
    movne   r1, #0x80000000
    ldmnefd sp!, {r4, pc}

    mov     r0, r4                      @ recover arg
    mov     r1, r4
    bl      __aeabi_fcmpeq              @ is arg == self?
    cmp     r0, #0                      @ zero == no
    moveq   r1, #0                      @ return zero for NaN
    ldmeqfd sp!, {r4, pc}

    mov     r0, r4                      @ recover arg
    bl      __aeabi_f2lz                @ convert float to long
    ldmfd   sp!, {r4, pc}
