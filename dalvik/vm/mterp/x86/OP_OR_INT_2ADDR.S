%verify "executed"
%include "x86/binop2addr.S" {"instr":"orl     %eax,(rFP,%ecx,4)"}
