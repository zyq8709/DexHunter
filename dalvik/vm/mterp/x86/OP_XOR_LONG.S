%verify "executed"
%include "x86/binopWide.S" {"instr1":"xorl (rFP,%ecx,4),rIBASE", "instr2":"xorl 4(rFP,%ecx,4),%eax"}
