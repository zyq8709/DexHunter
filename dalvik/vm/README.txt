Dalvik Virtual Machine


Source code rules of the road:

- All externally-visible function names must start with "dvm" to avoid
namespace clashes.  Use static functions when possible.

- Do not create static variables (globally or locally).  Do not create
global variables.  Keep everything with non-local lifespan in "gDvm",
defined in Globals.h, so that all global VM state is in one place.

- Use "startup" and "shutdown" functions to clean up gDvm.  The VM must
exit cleanly in valgrind.

- The primary target is ARM Linux.  Others are secondary, but must still
work correctly.

- Use of gcc-specific and C99 constructs is allowed.
