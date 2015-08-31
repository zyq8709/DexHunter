%verify "executed"
%include "mips/binopLit16.S" {"instr":"div zero, a0, a1; mfhi a0", "chkzero":"1"}
