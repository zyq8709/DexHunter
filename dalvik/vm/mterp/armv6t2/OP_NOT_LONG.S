%verify "executed"
%include "armv6t2/unopWide.S" {"preinstr":"mvn     r0, r0", "instr":"mvn     r1, r1"}
