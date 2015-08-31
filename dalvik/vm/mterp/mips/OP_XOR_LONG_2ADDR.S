%verify "executed"
%include "mips/binopWide2addr.S" {"preinstr":"xor a0, a0, a2", "instr":"xor a1, a1, a3"}
