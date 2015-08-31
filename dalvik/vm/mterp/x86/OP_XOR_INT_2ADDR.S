%verify "executed"
%include "x86/binop2addr.S" {"instr":"xorl     %eax,(rFP,%ecx,4)"}
