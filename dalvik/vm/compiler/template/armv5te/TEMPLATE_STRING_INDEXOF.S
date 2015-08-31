    /*
     * String's indexOf.
     *
     * Requires r0 to have been previously checked for null.  Will
     * return index of match of r1 in r0.
     *
     * IMPORTANT NOTE:
     *
     * This code relies on hard-coded offsets for string objects, and must be
     * kept in sync wth definitions in UtfString.h  See asm-constants.h
     *
     * On entry:
     *    r0:   string object pointer
     *    r1:   char to match
     *    r2:   Starting offset in string data
     */

    ldr    r3, [r0, #STRING_FIELDOFF_VALUE]
    ldr    r7, [r0, #STRING_FIELDOFF_OFFSET]
    ldr    r8, [r0, #STRING_FIELDOFF_COUNT]


    /*
     * At this point, we have:
     *    r1: char to match
     *    r2: starting offset
     *    r3: object pointer (final result -> r0)
     *    r7: offset
     *    r8: string length
     */

     /* Build pointer to start of string data */
     add   r3, #16
     add   r0, r3, r7, lsl #1

     /* Save a copy of starting data in r7 */
     mov   r7, r0

     /* Clamp start to [0..count] */
     cmp   r2, #0
     movlt r2, #0
     cmp   r2, r8
     movgt r2, r8

     /* Build pointer to start of data to compare and pre-bias */
     add   r0, r0, r2, lsl #1
     sub   r0, #2

     /* Compute iteration count */
     sub   r8, r2

     /*
      * At this point we have:
      *   r0: start of data to test
      *   r1: chat to compare
      *   r8: iteration count
      *   r7: original start of string
      *   r3, r4, r9, r10, r11, r12 available for loading string data
      */

    subs  r8, #4
    blt   indexof_remainder

indexof_loop4:
    ldrh  r3, [r0, #2]!
    ldrh  r4, [r0, #2]!
    ldrh  r10, [r0, #2]!
    ldrh  r11, [r0, #2]!
    cmp   r3, r1
    beq   match_0
    cmp   r4, r1
    beq   match_1
    cmp   r10, r1
    beq   match_2
    cmp   r11, r1
    beq   match_3
    subs  r8, #4
    bge   indexof_loop4

indexof_remainder:
    adds    r8, #4
    beq     indexof_nomatch

indexof_loop1:
    ldrh  r3, [r0, #2]!
    cmp   r3, r1
    beq   match_3
    subs  r8, #1
    bne   indexof_loop1

indexof_nomatch:
    mov   r0, #-1
    bx    lr

match_0:
    sub   r0, #6
    sub   r0, r7
    asr   r0, r0, #1
    bx    lr
match_1:
    sub   r0, #4
    sub   r0, r7
    asr   r0, r0, #1
    bx    lr
match_2:
    sub   r0, #2
    sub   r0, r7
    asr   r0, r0, #1
    bx    lr
match_3:
    sub   r0, r7
    asr   r0, r0, #1
    bx    lr
