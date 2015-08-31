%verify "executed"
%include "armv5te/OP_IPUT.S" {"prebarrier":"SMP_DMB_ST", "postbarrier":"SMP_DMB"}
