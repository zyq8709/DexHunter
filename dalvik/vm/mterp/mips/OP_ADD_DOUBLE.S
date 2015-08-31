%verify "executed"
%include "mips/binflopWide.S" {"instr":"JAL(__adddf3)", "instr_f":"add.d fv0, fa0, fa1"}
