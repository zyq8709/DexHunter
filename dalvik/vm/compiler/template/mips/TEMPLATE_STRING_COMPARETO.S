    /*
     * String's compareTo.
     *
     * Requires a0/a1 to have been previously checked for null.  Will
     * return negative if this's string is < comp, 0 if they are the
     * same and positive if >.
     *
     * IMPORTANT NOTE:
     *
     * This code relies on hard-coded offsets for string objects, and must be
     * kept in sync with definitions in UtfString.h.  See asm-constants.h
     *
     * On entry:
     *    a0:   this object pointer
     *    a1:   comp object pointer
     *
     */

     subu  v0, a0, a1                # Same?
     bnez  v0, 1f
     RETURN
1:
     lw    t0, STRING_FIELDOFF_OFFSET(a0)
     lw    t1, STRING_FIELDOFF_OFFSET(a1)
     lw    t2, STRING_FIELDOFF_COUNT(a0)
     lw    a2, STRING_FIELDOFF_COUNT(a1)
     lw    a0, STRING_FIELDOFF_VALUE(a0)
     lw    a1, STRING_FIELDOFF_VALUE(a1)

    /*
     * At this point, we have this/comp:
     *    offset: t0/t1
     *    count:  t2/a2
     *    value:  a0/a1
     * We're going to compute
     *    a3 <- countDiff
     *    a2 <- minCount
     */
     subu  a3, t2, a2                # a3<- countDiff
     sleu  t7, t2, a2
     movn  a2, t2, t7                # a2<- minCount

     /*
      * Note: data pointers point to first element.
      */
     addu  a0, 16                    # point to contents[0]
     addu  a1, 16                    # point to contents[0]

     /* Now, build pointers to the string data */
     sll   t7, t0, 1                 # multiply offset by 2
     addu  a0, a0, t7
     sll   t7, t1, 1                 # multiply offset by 2
     addu  a1, a1, t7

     /*
      * At this point we have:
      *   a0: *this string data
      *   a1: *comp string data
      *   a2: iteration count for comparison
      *   a3: value to return if the first part of the string is equal
      *   v0: reserved for result
      *   t0-t5 available for loading string data
      */

     subu  a2, 2
     bltz  a2, do_remainder2

     /*
      * Unroll the first two checks so we can quickly catch early mismatch
      * on long strings (but preserve incoming alignment)
      */
     lhu   t0, 0(a0)
     lhu   t1, 0(a1)
     subu  v0, t0, t1
     beqz  v0, 1f
     RETURN
1:
     lhu   t2, 2(a0)
     lhu   t3, 2(a1)
     subu  v0, t2, t3
     beqz  v0, 2f
     RETURN
2:
     addu  a0, 4                     # offset to contents[2]
     addu  a1, 4                     # offset to contents[2]
     li    t7, 28
     bgt   a2, t7, do_memcmp16
     subu  a2, 3
     bltz  a2, do_remainder

loopback_triple:
     lhu   t0, 0(a0)
     lhu   t1, 0(a1)
     subu  v0, t0, t1
     beqz  v0, 1f
     RETURN
1:
     lhu   t2, 2(a0)
     lhu   t3, 2(a1)
     subu  v0, t2, t3
     beqz  v0, 2f
     RETURN
2:
     lhu   t4, 4(a0)
     lhu   t5, 4(a1)
     subu  v0, t4, t5
     beqz  v0, 3f
     RETURN
3:
     addu  a0, 6                     # offset to contents[i+3]
     addu  a1, 6                     # offset to contents[i+3]
     subu  a2, 3
     bgez  a2, loopback_triple

do_remainder:
     addu  a2, 3
     beqz  a2, returnDiff

loopback_single:
     lhu   t0, 0(a0)
     lhu   t1, 0(a1)
     subu  v0, t0, t1
     bnez  v0, 1f
     addu  a0, 2                     # offset to contents[i+1]
     addu  a1, 2                     # offset to contents[i+1]
     subu  a2, 1
     bnez  a2, loopback_single

returnDiff:
     move  v0, a3
1:
     RETURN

do_remainder2:
     addu  a2, 2
     bnez  a2, loopback_single
     move  v0, a3
     RETURN

    /* Long string case */
do_memcmp16:
     move  rOBJ, a3                  # save return value if strings are equal
     JAL(__memcmp16)
     seq   t0, v0, zero
     movn  v0, rOBJ, t0              # overwrite return value if strings are equal
     RETURN
