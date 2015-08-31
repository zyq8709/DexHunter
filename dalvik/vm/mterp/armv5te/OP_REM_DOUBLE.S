%verify "executed"
/* EABI doesn't define a double remainder function, but libm does */
%include "armv5te/binopWide.S" {"instr":"bl      fmod"}
