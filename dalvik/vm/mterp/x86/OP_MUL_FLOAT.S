%verify "executed"
%include "x86/binflop.S" {"instr":"fmuls","load":"flds","store":"fstps"}
