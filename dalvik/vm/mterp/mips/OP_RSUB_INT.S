%verify "executed"
/* this op is "rsub-int", but can be thought of as "rsub-int/lit16" */
%include "mips/binopLit16.S" {"instr":"subu a0, a1, a0"}
