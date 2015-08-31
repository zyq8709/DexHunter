%verify "executed"
%verify "negative value is sign-extended"
@include "armv5te/OP_IGET.S" { "load":"ldrsb", "sqnum":"2" }
%include "armv5te/OP_IGET.S" { "load":"ldr", "sqnum":"2" }
