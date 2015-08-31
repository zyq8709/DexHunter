    /*
     * Throw an exception from JIT'ed code.
     * On entry:
     *    r0    Dalvik PC that raises the exception
     */
    b       .LhandleException
