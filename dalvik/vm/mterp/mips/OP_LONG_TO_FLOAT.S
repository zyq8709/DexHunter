%verify "executed"
%include "mips/unopNarrower.S" {"instr":"JAL(__floatdisf)", "instr_f":"JAL(__floatdisf)", "load":"LOAD64(rARG0, rARG1, a3)"}
