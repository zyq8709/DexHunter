%verify "executed"
%include "mips/OP_SGET.S" {"barrier":"SMP_DMB"}
