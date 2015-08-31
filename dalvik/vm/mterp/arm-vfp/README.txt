Instruction handlers that take advantage of ARM VFP.  These work with VFP
v2 and v3 (VFPLite).

The ARM code driving the floating-point calculations will run on ARMv5TE
and later.  It assumes that word alignment is sufficient for double-word
accesses (which is true for some ARMv5 and all ARMv6/v7), to avoid having
to transfer double-precision values in two steps.
