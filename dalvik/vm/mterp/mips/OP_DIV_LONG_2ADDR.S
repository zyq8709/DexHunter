%verify "executed"
#ifdef HAVE_LITTLE_ENDIAN
%include "mips/binopWide2addr.S" {"result0":"v0", "result1":"v1", "instr":"JAL(__divdi3)", "chkzero":"1"}
#else
%include "mips/binopWide2addr.S" {"arg0":"a1", "arg1":"a0", "arg2":"a3", "arg3":"a2", "result0":"v1", "result1":"v0", "instr":"JAL(__divdi3)", "chkzero":"1"}
#endif
