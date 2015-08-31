%verify "executed"
%include "mips/binopWide2addr.S" {"preinstr":"or a0, a0, a2", "instr":"or a1, a1, a3"}
