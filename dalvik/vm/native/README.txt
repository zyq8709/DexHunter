Internal native functions.

All of the functions defined here make direct use of VM functions or data
structures, so they can't be written with JNI and shouldn't really be in
a separate shared library.  Do not add additional functions here unless
they need to access VM internals directly.

All functions here either complete quickly or are used to enter a wait
state, so we don't set the thread status to THREAD_NATIVE when executing
these methods.  This means that the GC will wait for these functions
to finish.  DO NOT perform long operations or blocking I/O in here.
These methods should not be declared "synchronized", because we don't
check for that flag when issuing the call.

We use "late" binding on these, rather than explicit registration,
because it's easier to handle the core system classes that way.

The functions here use the DalvikNativeFunc prototype, but we can
also treat them as DalvikBridgeFunc, which takes two extra arguments.
The former represents the API that we're most likely to expose should
JNI performance be deemed insufficient.  The Bridge version is used as
an optimization for a few high-volume Object calls, and should generally
not be used as we may drop support for it at some point.
