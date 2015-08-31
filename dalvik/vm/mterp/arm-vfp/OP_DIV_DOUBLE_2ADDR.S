%verify "executed"
%include "arm-vfp/fbinopWide2addr.S" {"instr":"fdivd   d2, d0, d1"}
