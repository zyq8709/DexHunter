%verify "executed"
%include "mips/binflopWide2addr.S" {"instr":"JAL(fmod)", "instr_f":"JAL(fmod)"}
