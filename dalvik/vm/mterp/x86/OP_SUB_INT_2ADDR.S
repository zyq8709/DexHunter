%verify "executed"
%include "x86/binop2addr.S" {"instr":"subl     %eax,(rFP,%ecx,4)"}
