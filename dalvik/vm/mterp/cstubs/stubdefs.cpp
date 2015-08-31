/*
 * In the C mterp stubs, "goto" is a function call followed immediately
 * by a return.
 */

#define GOTO_TARGET_DECL(_target, ...)                                      \
    extern "C" void dvmMterp_##_target(Thread* self, ## __VA_ARGS__);

/* (void)xxx to quiet unused variable compiler warnings. */
#define GOTO_TARGET(_target, ...)                                           \
    void dvmMterp_##_target(Thread* self, ## __VA_ARGS__) {                 \
        u2 ref, vsrc1, vsrc2, vdst;                                         \
        u2 inst = FETCH(0);                                                 \
        const Method* methodToCall;                                         \
        StackSaveArea* debugSaveArea;                                       \
        (void)ref; (void)vsrc1; (void)vsrc2; (void)vdst; (void)inst;        \
        (void)methodToCall; (void)debugSaveArea;

#define GOTO_TARGET_END }

/*
 * Redefine what used to be local variable accesses into Thread struct
 * references.  (These are undefined down in "footer.cpp".)
 */
#define retval                  self->interpSave.retval
#define pc                      self->interpSave.pc
#define fp                      self->interpSave.curFrame
#define curMethod               self->interpSave.method
#define methodClassDex          self->interpSave.methodClassDex
#define debugTrackedRefStart    self->interpSave.debugTrackedRefStart

/* ugh */
#define STUB_HACK(x) x
#if defined(WITH_JIT)
#define JIT_STUB_HACK(x) x
#else
#define JIT_STUB_HACK(x)
#endif

/*
 * InterpSave's pc and fp must be valid when breaking out to a
 * "Reportxxx" routine.  Because the portable interpreter uses local
 * variables for these, we must flush prior.  Stubs, however, use
 * the interpSave vars directly, so this is a nop for stubs.
 */
#define PC_FP_TO_SELF()
#define PC_TO_SELF()

/*
 * Opcode handler framing macros.  Here, each opcode is a separate function
 * that takes a "self" argument and returns void.  We can't declare
 * these "static" because they may be called from an assembly stub.
 * (void)xxx to quiet unused variable compiler warnings.
 */
#define HANDLE_OPCODE(_op)                                                  \
    extern "C" void dvmMterp_##_op(Thread* self);                           \
    void dvmMterp_##_op(Thread* self) {                                     \
        u4 ref;                                                             \
        u2 vsrc1, vsrc2, vdst;                                              \
        u2 inst = FETCH(0);                                                 \
        (void)ref; (void)vsrc1; (void)vsrc2; (void)vdst; (void)inst;

#define OP_END }

/*
 * Like the "portable" FINISH, but don't reload "inst", and return to caller
 * when done.  Further, debugger/profiler checks are handled
 * before handler execution in mterp, so we don't do them here either.
 */
#if defined(WITH_JIT)
#define FINISH(_offset) {                                                   \
        ADJUST_PC(_offset);                                                 \
        if (self->interpBreak.ctl.subMode & kSubModeJitTraceBuild) {        \
            dvmCheckJit(pc, self);                                          \
        }                                                                   \
        return;                                                             \
    }
#else
#define FINISH(_offset) {                                                   \
        ADJUST_PC(_offset);                                                 \
        return;                                                             \
    }
#endif

#define FINISH_BKPT(_opcode)       /* FIXME? */
#define DISPATCH_EXTENDED(_opcode) /* FIXME? */

/*
 * The "goto label" statements turn into function calls followed by
 * return statements.  Some of the functions take arguments, which in the
 * portable interpreter are handled by assigning values to globals.
 */

#define GOTO_exceptionThrown()                                              \
    do {                                                                    \
        dvmMterp_exceptionThrown(self);                                     \
        return;                                                             \
    } while(false)

#define GOTO_returnFromMethod()                                             \
    do {                                                                    \
        dvmMterp_returnFromMethod(self);                                    \
        return;                                                             \
    } while(false)

#define GOTO_invoke(_target, _methodCallRange)                              \
    do {                                                                    \
        dvmMterp_##_target(self, _methodCallRange);                         \
        return;                                                             \
    } while(false)

#define GOTO_invokeMethod(_methodCallRange, _methodToCall, _vsrc1, _vdst)   \
    do {                                                                    \
        dvmMterp_invokeMethod(self, _methodCallRange, _methodToCall,        \
            _vsrc1, _vdst);                                                 \
        return;                                                             \
    } while(false)

/*
 * As a special case, "goto bail" turns into a longjmp.
 */
#define GOTO_bail()                                                         \
    dvmMterpStdBail(self)

/*
 * Periodically check for thread suspension.
 *
 * While we're at it, see if a debugger has attached or the profiler has
 * started.
 */
#define PERIODIC_CHECKS(_pcadj) {                              \
        if (dvmCheckSuspendQuick(self)) {                                   \
            EXPORT_PC();  /* need for precise GC */                         \
            dvmCheckSuspendPending(self);                                   \
        }                                                                   \
    }
