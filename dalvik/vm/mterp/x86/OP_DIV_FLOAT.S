%verify "executed"
%include "x86/binflop.S" {"instr":"fdivs","load":"flds","store":"fstps"}
