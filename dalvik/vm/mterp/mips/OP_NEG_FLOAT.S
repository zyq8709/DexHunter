%verify "executed"
%include "mips/unop.S" {"instr":"addu a0, a0, 0x80000000"}
