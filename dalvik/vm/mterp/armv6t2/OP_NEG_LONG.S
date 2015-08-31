%verify "executed"
%include "armv6t2/unopWide.S" {"preinstr":"rsbs    r0, r0, #0", "instr":"rsc     r1, r1, #0"}
