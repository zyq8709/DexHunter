%verify "executed"
%include "mips/binop.S" {"instr":"div zero, a0, a1; mfhi a0", "chkzero":"1"}
