%verify "executed"
%include "mips/binflop.S" {"instr":"JAL(__addsf3)", "instr_f":"add.s fv0, fa0, fa1"}
