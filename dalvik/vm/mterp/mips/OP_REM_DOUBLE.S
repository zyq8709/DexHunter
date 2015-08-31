%verify "executed"
%include "mips/binflopWide.S" {"instr":"JAL(fmod)", "instr_f":"JAL(fmod)"}
