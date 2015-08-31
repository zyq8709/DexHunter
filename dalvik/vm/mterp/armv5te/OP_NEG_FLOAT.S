%verify "executed"
%include "armv5te/unop.S" {"instr":"add     r0, r0, #0x80000000"}
