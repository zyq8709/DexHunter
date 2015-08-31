%verify "executed"
%include "mips/binflopWide2addr.S" {"instr":"JAL(__muldf3)", "instr_f":"mul.d fv0, fa0, fa1"}
