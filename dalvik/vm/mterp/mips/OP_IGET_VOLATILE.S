%verify "executed"
%include "mips/OP_IGET.S" {"barrier":"SMP_DMB"}
