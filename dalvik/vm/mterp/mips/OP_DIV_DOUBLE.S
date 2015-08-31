%verify "executed"
%include "mips/binflopWide.S" {"instr":"JAL(__divdf3)", "instr_f":"div.d fv0, fa0, fa1"}
