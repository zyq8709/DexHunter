%verify "executed"
%include "arm-vfp/fbinopWide2addr.S" {"instr":"fmuld   d2, d0, d1"}
