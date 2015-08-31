%verify "executed"
%verify "basic lt, gt, eq */
%verify "left arg NaN"
%verify "right arg NaN"
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * int compare(x, y) {
     *     if (x == y) {
     *         return 0;
     *     } else if (x > y) {
     *         return 1;
     *     } else if (x < y) {
     *         return -1;
     *     } else {
     *         return -1;
     *     }
     * }
     * On entry:
     *    r0 = &op1 [vBB]
     *    r1 = &op2 [vCC]
     */
    /* op vAA, vBB, vCC */
    fldd    d0, [r0]                    @ d0<- vBB
    fldd    d1, [r1]                    @ d1<- vCC
    fcmped  d0, d1                      @ compare (vBB, vCC)
    mvn     r0, #0                      @ r0<- -1 (default)
    fmstat                              @ export status flags
    movgt   r0, #1                      @ (greater than) r0<- 1
    moveq   r0, #0                      @ (equal) r0<- 0
    bx      lr
