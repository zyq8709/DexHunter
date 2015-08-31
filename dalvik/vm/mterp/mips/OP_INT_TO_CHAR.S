%verify "executed"
%include "mips/unop.S" {"preinstr":"", "instr":"and a0, 0xffff"}
