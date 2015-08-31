%verify "executed"
%include "x86/bindivLit16.S" {"result":"%eax","special":"$0x80000000"}
