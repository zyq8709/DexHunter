%verify "executed"
@include "armv5te/OP_IGET.S" { "load":"ldrb", "sqnum":"1" }
%include "armv5te/OP_IGET.S" { "load":"ldr", "sqnum":"1" }
