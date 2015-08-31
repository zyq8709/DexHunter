%verify "executed"
%include "x86/bindivLit8.S" {"result":"%eax","special":"$0x80000000"}
