%verify "executed"
/* this op is "rsub-int", but can be thought of as "rsub-int/lit16" */
%include "armv6t2/binopLit16.S" {"instr":"rsb     r0, r0, r1"}
