%verify "executed"
%verify "large values are not sign-extended"
@include "armv5te/OP_IGET.S" { "load":"ldrh", "sqnum":"3" }
%include "armv5te/OP_IGET.S" { "load":"ldr", "sqnum":"3" }
