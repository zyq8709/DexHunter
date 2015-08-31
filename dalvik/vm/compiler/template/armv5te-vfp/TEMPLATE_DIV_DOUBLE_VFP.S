%verify "executed"
%include "armv5te-vfp/fbinopWide.S" {"instr":"fdivd   d2, d0, d1"}
