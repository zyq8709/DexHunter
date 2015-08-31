/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "runtime.h"

#include <signal.h>
#include <string.h>
#include <sys/utsname.h>

#include "base/logging.h"
#include "base/mutex.h"
#include "base/stringprintf.h"
#include "thread.h"
#include "utils.h"

namespace art {

struct Backtrace {
  void Dump(std::ostream& os) {
    DumpNativeStack(os, GetTid(), "\t", true);
  }
};

struct OsInfo {
  void Dump(std::ostream& os) {
    utsname info;
    uname(&info);
    // Linux 2.6.38.8-gg784 (x86_64)
    // Darwin 11.4.0 (x86_64)
    os << info.sysname << " " << info.release << " (" << info.machine << ")";
  }
};

static const char* GetSignalName(int signal_number) {
  switch (signal_number) {
    case SIGABRT: return "SIGABRT";
    case SIGBUS: return "SIGBUS";
    case SIGFPE: return "SIGFPE";
    case SIGILL: return "SIGILL";
    case SIGPIPE: return "SIGPIPE";
    case SIGSEGV: return "SIGSEGV";
#if defined(SIGSTKFLT)
    case SIGSTKFLT: return "SIGSTKFLT";
#endif
    case SIGTRAP: return "SIGTRAP";
  }
  return "??";
}

static const char* GetSignalCodeName(int signal_number, int signal_code) {
  // Try the signal-specific codes...
  switch (signal_number) {
    case SIGILL:
      switch (signal_code) {
        case ILL_ILLOPC: return "ILL_ILLOPC";
        case ILL_ILLOPN: return "ILL_ILLOPN";
        case ILL_ILLADR: return "ILL_ILLADR";
        case ILL_ILLTRP: return "ILL_ILLTRP";
        case ILL_PRVOPC: return "ILL_PRVOPC";
        case ILL_PRVREG: return "ILL_PRVREG";
        case ILL_COPROC: return "ILL_COPROC";
        case ILL_BADSTK: return "ILL_BADSTK";
      }
      break;
    case SIGBUS:
      switch (signal_code) {
        case BUS_ADRALN: return "BUS_ADRALN";
        case BUS_ADRERR: return "BUS_ADRERR";
        case BUS_OBJERR: return "BUS_OBJERR";
      }
      break;
    case SIGFPE:
      switch (signal_code) {
        case FPE_INTDIV: return "FPE_INTDIV";
        case FPE_INTOVF: return "FPE_INTOVF";
        case FPE_FLTDIV: return "FPE_FLTDIV";
        case FPE_FLTOVF: return "FPE_FLTOVF";
        case FPE_FLTUND: return "FPE_FLTUND";
        case FPE_FLTRES: return "FPE_FLTRES";
        case FPE_FLTINV: return "FPE_FLTINV";
        case FPE_FLTSUB: return "FPE_FLTSUB";
      }
      break;
    case SIGSEGV:
      switch (signal_code) {
        case SEGV_MAPERR: return "SEGV_MAPERR";
        case SEGV_ACCERR: return "SEGV_ACCERR";
      }
      break;
    case SIGTRAP:
      switch (signal_code) {
        case TRAP_BRKPT: return "TRAP_BRKPT";
        case TRAP_TRACE: return "TRAP_TRACE";
      }
      break;
  }
  // Then the other codes...
  switch (signal_code) {
    case SI_USER:     return "SI_USER";
#if defined(SI_KERNEL)
    case SI_KERNEL:   return "SI_KERNEL";
#endif
    case SI_QUEUE:    return "SI_QUEUE";
    case SI_TIMER:    return "SI_TIMER";
    case SI_MESGQ:    return "SI_MESGQ";
    case SI_ASYNCIO:  return "SI_ASYNCIO";
#if defined(SI_SIGIO)
    case SI_SIGIO:    return "SI_SIGIO";
#endif
#if defined(SI_TKILL)
    case SI_TKILL:    return "SI_TKILL";
#endif
  }
  // Then give up...
  return "?";
}

struct UContext {
  explicit UContext(void* raw_context) : context(reinterpret_cast<ucontext_t*>(raw_context)->uc_mcontext) {}

  void Dump(std::ostream& os) {
    // TODO: support non-x86 hosts (not urgent because this code doesn't run on targets).
#if defined(__APPLE__)
    DumpRegister32(os, "eax", context->__ss.__eax);
    DumpRegister32(os, "ebx", context->__ss.__ebx);
    DumpRegister32(os, "ecx", context->__ss.__ecx);
    DumpRegister32(os, "edx", context->__ss.__edx);
    os << '\n';

    DumpRegister32(os, "edi", context->__ss.__edi);
    DumpRegister32(os, "esi", context->__ss.__esi);
    DumpRegister32(os, "ebp", context->__ss.__ebp);
    DumpRegister32(os, "esp", context->__ss.__esp);
    os << '\n';

    DumpRegister32(os, "eip", context->__ss.__eip);
    os << "                   ";
    DumpRegister32(os, "eflags", context->__ss.__eflags);
    DumpX86Flags(os, context->__ss.__eflags);
    os << '\n';

    DumpRegister32(os, "cs",  context->__ss.__cs);
    DumpRegister32(os, "ds",  context->__ss.__ds);
    DumpRegister32(os, "es",  context->__ss.__es);
    DumpRegister32(os, "fs",  context->__ss.__fs);
    os << '\n';
    DumpRegister32(os, "gs",  context->__ss.__gs);
    DumpRegister32(os, "ss",  context->__ss.__ss);
#else
    DumpRegister32(os, "eax", context.gregs[REG_EAX]);
    DumpRegister32(os, "ebx", context.gregs[REG_EBX]);
    DumpRegister32(os, "ecx", context.gregs[REG_ECX]);
    DumpRegister32(os, "edx", context.gregs[REG_EDX]);
    os << '\n';

    DumpRegister32(os, "edi", context.gregs[REG_EDI]);
    DumpRegister32(os, "esi", context.gregs[REG_ESI]);
    DumpRegister32(os, "ebp", context.gregs[REG_EBP]);
    DumpRegister32(os, "esp", context.gregs[REG_ESP]);
    os << '\n';

    DumpRegister32(os, "eip", context.gregs[REG_EIP]);
    os << "                   ";
    DumpRegister32(os, "eflags", context.gregs[REG_EFL]);
    DumpX86Flags(os, context.gregs[REG_EFL]);
    os << '\n';

    DumpRegister32(os, "cs",  context.gregs[REG_CS]);
    DumpRegister32(os, "ds",  context.gregs[REG_DS]);
    DumpRegister32(os, "es",  context.gregs[REG_ES]);
    DumpRegister32(os, "fs",  context.gregs[REG_FS]);
    os << '\n';
    DumpRegister32(os, "gs",  context.gregs[REG_GS]);
    DumpRegister32(os, "ss",  context.gregs[REG_SS]);
#endif
  }

  void DumpRegister32(std::ostream& os, const char* name, uint32_t value) {
    os << StringPrintf(" %6s: 0x%08x", name, value);
  }

  void DumpX86Flags(std::ostream& os, uint32_t flags) {
    os << " [";
    if ((flags & (1 << 0)) != 0) {
      os << " CF";
    }
    if ((flags & (1 << 2)) != 0) {
      os << " PF";
    }
    if ((flags & (1 << 4)) != 0) {
      os << " AF";
    }
    if ((flags & (1 << 6)) != 0) {
      os << " ZF";
    }
    if ((flags & (1 << 7)) != 0) {
      os << " SF";
    }
    if ((flags & (1 << 8)) != 0) {
      os << " TF";
    }
    if ((flags & (1 << 9)) != 0) {
      os << " IF";
    }
    if ((flags & (1 << 10)) != 0) {
      os << " DF";
    }
    if ((flags & (1 << 11)) != 0) {
      os << " OF";
    }
    os << " ]";
  }

  mcontext_t& context;
};

void HandleUnexpectedSignal(int signal_number, siginfo_t* info, void* raw_context) {
  static bool handlingUnexpectedSignal = false;
  if (handlingUnexpectedSignal) {
    LogMessageData data(__FILE__, __LINE__, INTERNAL_FATAL, -1);
    LogMessage::LogLine(data, "HandleUnexpectedSignal reentered\n");
    _exit(1);
  }
  handlingUnexpectedSignal = true;

  gAborting++;  // set before taking any locks
  MutexLock mu(Thread::Current(), *Locks::unexpected_signal_lock_);

  bool has_address = (signal_number == SIGILL || signal_number == SIGBUS ||
                      signal_number == SIGFPE || signal_number == SIGSEGV);

  OsInfo os_info;
  const char* cmd_line = GetCmdLine();
  if (cmd_line == NULL) {
    cmd_line = "<unset>";  // Because no-one called InitLogging.
  }
  pid_t tid = GetTid();
  std::string thread_name(GetThreadName(tid));
  UContext thread_context(raw_context);
  Backtrace thread_backtrace;

  LOG(INTERNAL_FATAL) << "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n"
                      << StringPrintf("Fatal signal %d (%s), code %d (%s)",
                                      signal_number, GetSignalName(signal_number),
                                      info->si_code,
                                      GetSignalCodeName(signal_number, info->si_code))
                      << (has_address ? StringPrintf(" fault addr %p", info->si_addr) : "") << "\n"
                      << "OS: " << Dumpable<OsInfo>(os_info) << "\n"
                      << "Cmdline: " << cmd_line << "\n"
                      << "Thread: " << tid << " \"" << thread_name << "\"\n"
                      << "Registers:\n" << Dumpable<UContext>(thread_context) << "\n"
                      << "Backtrace:\n" << Dumpable<Backtrace>(thread_backtrace);

  if (getenv("debug_db_uid") != NULL || getenv("art_wait_for_gdb_on_crash") != NULL) {
    LOG(INTERNAL_FATAL) << "********************************************************\n"
                        << "* Process " << getpid() << " thread " << tid << " \"" << thread_name << "\""
                        << " has been suspended while crashing.\n"
                        << "* Attach gdb:\n"
                        << "*     gdb -p " << tid << "\n"
                        << "********************************************************\n";
    // Wait for debugger to attach.
    while (true) {
    }
  }

  // Remove our signal handler for this signal...
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_DFL;
  sigaction(signal_number, &action, NULL);
  // ...and re-raise so we die with the appropriate status.
  kill(getpid(), signal_number);
}

void Runtime::InitPlatformSignalHandlers() {
  // On the host, we don't have debuggerd to dump a stack for us when something unexpected happens.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_sigaction = HandleUnexpectedSignal;
  // Use the three-argument sa_sigaction handler.
  action.sa_flags |= SA_SIGINFO;
  // Use the alternate signal stack so we can catch stack overflows.
  action.sa_flags |= SA_ONSTACK;

  int rc = 0;
  rc += sigaction(SIGABRT, &action, NULL);
  rc += sigaction(SIGBUS, &action, NULL);
  rc += sigaction(SIGFPE, &action, NULL);
  rc += sigaction(SIGILL, &action, NULL);
  rc += sigaction(SIGPIPE, &action, NULL);
  rc += sigaction(SIGSEGV, &action, NULL);
#if defined(SIGSTKFLT)
  rc += sigaction(SIGSTKFLT, &action, NULL);
#endif
  rc += sigaction(SIGTRAP, &action, NULL);
  CHECK_EQ(rc, 0);
}

}  // namespace art
