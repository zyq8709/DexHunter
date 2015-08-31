%verify "executed"
%include "mips/binop.S" {"instr":"mul a0, a0, a1"}
