%verify "executed"
%include "mips/OP_IPUT.S" {"prebarrier":"SMP_DMB_ST", "postbarrier":"SMP_DMB"}
