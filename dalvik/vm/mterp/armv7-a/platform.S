/*
 * ===========================================================================
 *  CPU-version-specific defines
 * ===========================================================================
 */

#if !defined(ANDROID_SMP)
# error "Must define ANDROID_SMP"
#endif

/*
 * Macro for data memory barrier.
 */
.macro  SMP_DMB
#if ANDROID_SMP != 0
    dmb
#else
    /* not SMP */
#endif
.endm

/*
 * Macro for data memory barrier (store/store variant).
 */
.macro  SMP_DMB_ST
#if ANDROID_SMP != 0
    dmb     st
#else
    /* not SMP */
#endif
.endm
