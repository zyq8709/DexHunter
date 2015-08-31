%verify "executed"
%include "arm-vfp/fbinop2addr.S" {"instr":"fdivs   s2, s0, s1"}
