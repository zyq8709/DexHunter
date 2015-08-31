%verify "executed"
%include "x86/binflop2addr.S" {"instr":"fdivs","load":"flds","store":"fstps"}
