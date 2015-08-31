%verify "executed"
%verify "negative value is sign-extended"
@include "armv5te/OP_IGET.S" { "load":"ldrsh", "sqnum":"4" }
%include "armv5te/OP_IGET.S" { "load":"ldr", "sqnum":"4" }
