%verify "executed"
%include "mips/binop2addr.S" {"preinstr":"and a1, a1, 31", "instr":"sra a0, a0, a1"}
