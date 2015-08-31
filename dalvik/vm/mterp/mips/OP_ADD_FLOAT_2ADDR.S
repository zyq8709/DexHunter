%verify "executed"
%include "mips/binflop2addr.S" {"instr":"JAL(__addsf3)", "instr_f":"add.s fv0, fa0, fa1"}
