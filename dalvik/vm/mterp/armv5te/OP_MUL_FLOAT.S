%verify "executed"
%include "armv5te/binop.S" {"instr":"bl      __aeabi_fmul"}
