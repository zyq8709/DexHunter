/*
 * ===========================================================================
 *  CPU-version-specific defines
 * ===========================================================================
 */

/*
 * Macro for data memory barrier; not meaningful pre-ARMv6K.
 */
.macro  SMP_DMB
.endm

/*
 * Macro for data memory barrier; not meaningful pre-ARMv6K.
 */
.macro  SMP_DMB_ST
.endm
