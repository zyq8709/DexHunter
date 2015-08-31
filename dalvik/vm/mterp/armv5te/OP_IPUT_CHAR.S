%verify "executed"
@include "armv5te/OP_IPUT.S" { "store":"strh", "sqnum":"3" }
%include "armv5te/OP_IPUT.S" { "store":"str", "sqnum":"3" }
