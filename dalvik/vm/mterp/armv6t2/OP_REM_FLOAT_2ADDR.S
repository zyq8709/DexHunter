%verify "executed"
/* EABI doesn't define a float remainder function, but libm does */
%include "armv6t2/binop2addr.S" {"instr":"bl      fmodf"}
