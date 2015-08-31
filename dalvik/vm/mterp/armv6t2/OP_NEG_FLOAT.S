%verify "executed"
%include "armv6t2/unop.S" {"instr":"add     r0, r0, #0x80000000"}
