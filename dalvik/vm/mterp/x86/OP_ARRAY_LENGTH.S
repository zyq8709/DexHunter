%verify "executed"
    /*
     * Return the length of an array.
     */
   mov      rINST,%eax                # eax<- BA
   sarl     $$4,rINST                 # rINST<- B
   GET_VREG_R %ecx rINST              # ecx<- vB (object ref)
   andb     $$0xf,%al                 # eax<- A
   testl    %ecx,%ecx                 # is null?
   je       common_errNullObject
   movl     offArrayObject_length(%ecx),rINST
   FETCH_INST_OPCODE 1 %ecx
   ADVANCE_PC 1
   SET_VREG rINST %eax
   GOTO_NEXT_R %ecx
