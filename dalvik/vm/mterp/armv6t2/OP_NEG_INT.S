%verify "executed"
%include "armv6t2/unop.S" {"instr":"rsb     r0, r0, #0"}
