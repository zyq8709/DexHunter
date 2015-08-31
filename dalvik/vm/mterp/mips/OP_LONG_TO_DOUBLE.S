%verify "executed"
%include "mips/unflopWide.S" {"instr":"JAL(__floatdidf)", "ld_arg":"LOAD64(rARG0, rARG1, a3)"}
