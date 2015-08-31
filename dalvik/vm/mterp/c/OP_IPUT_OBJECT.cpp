/*
 * The VM spec says we should verify that the reference being stored into
 * the field is assignment compatible.  In practice, many popular VMs don't
 * do this because it slows down a very common operation.  It's not so bad
 * for us, since "dexopt" quickens it whenever possible, but it's still an
 * issue.
 *
 * To make this spec-complaint, we'd need to add a ClassObject pointer to
 * the Field struct, resolve the field's type descriptor at link or class
 * init time, and then verify the type here.
 */
HANDLE_IPUT_X(OP_IPUT_OBJECT,           "-object", Object, _AS_OBJECT)
OP_END
