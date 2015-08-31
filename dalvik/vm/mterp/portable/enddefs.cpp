/*--- end of opcodes ---*/

bail:
    ILOGD("|-- Leaving interpreter loop");      // note "curMethod" may be NULL

    self->interpSave.retval = retval;
}
