%verify "executed"
%include "mips/unop.S" {"preinstr":"sll a0, a0, 24", "instr":"sra a0, a0, 24"}
