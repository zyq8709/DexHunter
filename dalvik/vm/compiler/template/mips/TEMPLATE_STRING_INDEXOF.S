    /*
     * String's indexOf.
     *
     * Requires a0 to have been previously checked for null.  Will
     * return index of match of a1 in v0.
     *
     * IMPORTANT NOTE:
     *
     * This code relies on hard-coded offsets for string objects, and must be
     * kept in sync wth definitions in UtfString.h  See asm-constants.h
     *
     * On entry:
     *    a0:   string object pointer
     *    a1:   char to match
     *    a2:   Starting offset in string data
     */

     lw    t0, STRING_FIELDOFF_OFFSET(a0)
     lw    t1, STRING_FIELDOFF_COUNT(a0)
     lw    v0, STRING_FIELDOFF_VALUE(a0)

    /*
     * At this point, we have:
     *    v0: object pointer
     *    a1: char to match
     *    a2: starting offset
     *    t0: offset
     *    t1: string length
     */

    /* Point to first element */
     addu  v0, 16                    # point to contents[0]

    /* Build pointer to start of string data */
     sll   t7, t0, 1                 # multiply offset by 2
     addu  v0, v0, t7

    /* Save a copy of starting data in v1 */
     move  v1, v0

    /* Clamp start to [0..count] */
     slt   t7, a2, zero
     movn  a2, zero, t7
     sgt   t7, a2, t1
     movn  a2, t1, t7

    /* Build pointer to start of data to compare */
     sll   t7, a2, 1                # multiply offset by 2
     addu  v0, v0, t7

    /* Compute iteration count */
     subu  a3, t1, a2

    /*
     * At this point we have:
     *   v0: start of data to test
     *   a1: char to compare
     *   a3: iteration count
     *   v1: original start of string
     *   t0-t7 available for loading string data
     */
     subu  a3, 4
     bltz  a3, indexof_remainder

indexof_loop4:
     lhu   t0, 0(v0)
     beq   t0, a1, match_0
     lhu   t0, 2(v0)
     beq   t0, a1, match_1
     lhu   t0, 4(v0)
     beq   t0, a1, match_2
     lhu   t0, 6(v0)
     beq   t0, a1, match_3
     addu  v0, 8                     # offset to contents[i+4]
     subu  a3, 4
     bgez  a3, indexof_loop4

indexof_remainder:
     addu  a3, 4
     beqz  a3, indexof_nomatch

indexof_loop1:
     lhu   t0, 0(v0)
     beq   t0, a1, match_0
     addu  v0, 2                     # offset to contents[i+1]
     subu  a3, 1
     bnez  a3, indexof_loop1

indexof_nomatch:
     li    v0, -1
     RETURN

match_0:
     subu  v0, v1
     sra   v0, v0, 1                 # divide by 2
     RETURN
match_1:
     addu  v0, 2
     subu  v0, v1
     sra   v0, v0, 1                 # divide by 2
     RETURN
match_2:
     addu  v0, 4
     subu  v0, v1
     sra   v0, v0, 1                 # divide by 2
     RETURN
match_3:
     addu  v0, 6
     subu  v0, v1
     sra   v0, v0, 1                 # divide by 2
     RETURN
