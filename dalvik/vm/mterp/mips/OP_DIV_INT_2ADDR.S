%verify "executed"
%include "mips/binop2addr.S" {"instr":"div zero, a0, a1; mflo a0", "chkzero":"1"}
