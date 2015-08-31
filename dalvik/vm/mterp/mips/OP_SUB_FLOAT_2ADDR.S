%verify "executed"
%include "mips/binflop2addr.S" {"instr":"JAL(__subsf3)", "instr_f":"sub.s fv0, fa0, fa1"}
