%verify "executed"
@include "armv5te/OP_IPUT.S" { "store":"strh", "sqnum":"4" }
%include "armv5te/OP_IPUT.S" { "store":"str", "sqnum":"4" }
