%verify "executed"
%include "armv5te/unopWide.S" {"preinstr":"mvn     r0, r0", "instr":"mvn     r1, r1"}
