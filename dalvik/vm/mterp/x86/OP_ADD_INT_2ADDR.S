%verify "executed"
%include "x86/binop2addr.S" {"instr":"addl     %eax,(rFP,%ecx,4)"}
