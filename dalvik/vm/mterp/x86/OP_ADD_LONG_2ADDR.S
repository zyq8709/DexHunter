%verify "executed"
%include "x86/binopWide2addr.S" {"instr1":"addl %eax,(rFP,rINST,4)","instr2":"adcl %ecx,4(rFP,rINST,4)"}
