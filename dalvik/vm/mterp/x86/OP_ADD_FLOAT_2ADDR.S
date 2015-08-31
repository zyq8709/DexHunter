%verify "executed"
%include "x86/binflop2addr.S" {"instr":"fadds","load":"flds","store":"fstps"}
