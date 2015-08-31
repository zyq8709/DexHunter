%verify "executed"
%include "mips/fbinop.S" {"instr":"JAL(__addsf3)", "instr_f":"add.s fv0, fa0, fa1"}
