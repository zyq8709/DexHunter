%verify "executed"
%include "x86/binflop2addr.S" {"instr":"fmuls","load":"flds","store":"fstps"}
