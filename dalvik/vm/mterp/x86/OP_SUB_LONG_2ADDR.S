%verify "executed"
%include "x86/binopWide2addr.S" {"instr1":"subl %eax,(rFP,rINST,4)","instr2":"sbbl %ecx,4(rFP,rINST,4)"}
