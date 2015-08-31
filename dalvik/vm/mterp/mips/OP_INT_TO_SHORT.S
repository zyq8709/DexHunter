%verify "executed"
%include "mips/unop.S" {"preinstr":"sll a0, 16", "instr":"sra a0, 16"}
