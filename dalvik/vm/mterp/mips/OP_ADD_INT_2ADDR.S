%verify "executed"
%include "mips/binop2addr.S" {"instr":"addu a0, a0, a1"}
