%verify "executed"
%include "x86/binop2addr.S" {"instr":"andl     %eax,(rFP,%ecx,4)"}
