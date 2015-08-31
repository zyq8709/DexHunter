%verify "executed"
%include "mips/binopWide.S" {"preinstr":"and a0, a0, a2", "instr":"and a1, a1, a3"}
