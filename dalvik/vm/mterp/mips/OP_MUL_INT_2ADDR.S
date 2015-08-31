%verify "executed"
%include "mips/binop2addr.S" {"instr":"mul a0, a0, a1"}
