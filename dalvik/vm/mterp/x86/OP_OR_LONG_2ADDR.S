%verify "executed"
%include "x86/binopWide2addr.S" {"instr1":"orl %eax,(rFP,rINST,4)","instr2":"orl %ecx,4(rFP,rINST,4)"}
