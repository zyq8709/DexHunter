%verify "executed"
%include "mips/fbinopWide.S" {"instr":"JAL(__subdf3)","instr_f":"sub.d fv0, fa0, fa1"}
