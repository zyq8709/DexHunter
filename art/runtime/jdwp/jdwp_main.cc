/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "atomic.h"
#include "base/logging.h"
#include "debugger.h"
#include "jdwp/jdwp_priv.h"
#include "scoped_thread_state_change.h"

namespace art {

namespace JDWP {

static void* StartJdwpThread(void* arg);

/*
 * JdwpNetStateBase class implementation
 */
JdwpNetStateBase::JdwpNetStateBase(JdwpState* state)
    : state_(state), socket_lock_("JdwpNetStateBase lock", kJdwpSocketLock) {
  clientSock = -1;
  wake_pipe_[0] = -1;
  wake_pipe_[1] = -1;
  input_count_ = 0;
  awaiting_handshake_ = false;
}

JdwpNetStateBase::~JdwpNetStateBase() {
  if (wake_pipe_[0] != -1) {
    close(wake_pipe_[0]);
    wake_pipe_[0] = -1;
  }
  if (wake_pipe_[1] != -1) {
    close(wake_pipe_[1]);
    wake_pipe_[1] = -1;
  }
}

bool JdwpNetStateBase::MakePipe() {
  if (pipe(wake_pipe_) == -1) {
    PLOG(ERROR) << "pipe failed";
    return false;
  }
  return true;
}

void JdwpNetStateBase::WakePipe() {
  // If we might be sitting in select, kick us loose.
  if (wake_pipe_[1] != -1) {
    VLOG(jdwp) << "+++ writing to wake pipe";
    TEMP_FAILURE_RETRY(write(wake_pipe_[1], "", 1));
  }
}

void JdwpNetStateBase::ConsumeBytes(size_t count) {
  CHECK_GT(count, 0U);
  CHECK_LE(count, input_count_);

  if (count == input_count_) {
    input_count_ = 0;
    return;
  }

  memmove(input_buffer_, input_buffer_ + count, input_count_ - count);
  input_count_ -= count;
}

bool JdwpNetStateBase::HaveFullPacket() {
  if (awaiting_handshake_) {
    return (input_count_ >= kMagicHandshakeLen);
  }
  if (input_count_ < 4) {
    return false;
  }
  uint32_t length = Get4BE(input_buffer_);
  return (input_count_ >= length);
}

bool JdwpNetStateBase::IsAwaitingHandshake() {
  return awaiting_handshake_;
}

void JdwpNetStateBase::SetAwaitingHandshake(bool new_state) {
  awaiting_handshake_ = new_state;
}

bool JdwpNetStateBase::IsConnected() {
  return clientSock >= 0;
}

// Close a connection from a debugger (which may have already dropped us).
// Resets the state so we're ready to receive a new connection.
// Only called from the JDWP thread.
void JdwpNetStateBase::Close() {
  if (clientSock < 0) {
    return;
  }

  VLOG(jdwp) << "+++ closing JDWP connection on fd " << clientSock;

  close(clientSock);
  clientSock = -1;
}

/*
 * Write a packet. Grabs a mutex to assure atomicity.
 */
ssize_t JdwpNetStateBase::WritePacket(ExpandBuf* pReply) {
  MutexLock mu(Thread::Current(), socket_lock_);
  return TEMP_FAILURE_RETRY(write(clientSock, expandBufGetBuffer(pReply), expandBufGetLength(pReply)));
}

/*
 * Write a buffered packet. Grabs a mutex to assure atomicity.
 */
ssize_t JdwpNetStateBase::WriteBufferedPacket(const std::vector<iovec>& iov) {
  MutexLock mu(Thread::Current(), socket_lock_);
  return TEMP_FAILURE_RETRY(writev(clientSock, &iov[0], iov.size()));
}

bool JdwpState::IsConnected() {
  return netState != NULL && netState->IsConnected();
}

void JdwpState::SendBufferedRequest(uint32_t type, const std::vector<iovec>& iov) {
  if (netState->clientSock < 0) {
    // Can happen with some DDMS events.
    VLOG(jdwp) << "Not sending JDWP packet: no debugger attached!";
    return;
  }

  size_t expected = 0;
  for (size_t i = 0; i < iov.size(); ++i) {
    expected += iov[i].iov_len;
  }

  errno = 0;
  ssize_t actual = netState->WriteBufferedPacket(iov);
  if (static_cast<size_t>(actual) != expected) {
    PLOG(ERROR) << StringPrintf("Failed to send JDWP packet %c%c%c%c to debugger (%d of %d)",
                                static_cast<uint8_t>(type >> 24),
                                static_cast<uint8_t>(type >> 16),
                                static_cast<uint8_t>(type >> 8),
                                static_cast<uint8_t>(type),
                                actual, expected);
  }
}

void JdwpState::SendRequest(ExpandBuf* pReq) {
  if (netState->clientSock < 0) {
    // Can happen with some DDMS events.
    VLOG(jdwp) << "Not sending JDWP packet: no debugger attached!";
    return;
  }

  errno = 0;
  ssize_t actual = netState->WritePacket(pReq);
  if (static_cast<size_t>(actual) != expandBufGetLength(pReq)) {
    PLOG(ERROR) << StringPrintf("Failed to send JDWP packet to debugger (%d of %d)",
                                actual, expandBufGetLength(pReq));
  }
}

/*
 * Get the next "request" serial number.  We use this when sending
 * packets to the debugger.
 */
uint32_t JdwpState::NextRequestSerial() {
  return request_serial_++;
}

/*
 * Get the next "event" serial number.  We use this in the response to
 * message type EventRequest.Set.
 */
uint32_t JdwpState::NextEventSerial() {
  return event_serial_++;
}

JdwpState::JdwpState(const JdwpOptions* options)
    : options_(options),
      thread_start_lock_("JDWP thread start lock", kJdwpStartLock),
      thread_start_cond_("JDWP thread start condition variable", thread_start_lock_),
      pthread_(0),
      thread_(NULL),
      debug_thread_started_(false),
      debug_thread_id_(0),
      run(false),
      netState(NULL),
      attach_lock_("JDWP attach lock", kJdwpAttachLock),
      attach_cond_("JDWP attach condition variable", attach_lock_),
      last_activity_time_ms_(0),
      request_serial_(0x10000000),
      event_serial_(0x20000000),
      event_list_lock_("JDWP event list lock", kJdwpEventListLock),
      event_list_(NULL),
      event_list_size_(0),
      event_thread_lock_("JDWP event thread lock"),
      event_thread_cond_("JDWP event thread condition variable", event_thread_lock_),
      event_thread_id_(0),
      ddm_is_active_(false),
      should_exit_(false),
      exit_status_(0) {
}

/*
 * Initialize JDWP.
 *
 * Does not return until JDWP thread is running, but may return before
 * the thread is accepting network connections.
 */
JdwpState* JdwpState::Create(const JdwpOptions* options) {
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertNotHeld(self);
  UniquePtr<JdwpState> state(new JdwpState(options));
  switch (options->transport) {
  case kJdwpTransportSocket:
    InitSocketTransport(state.get(), options);
    break;
#ifdef HAVE_ANDROID_OS
  case kJdwpTransportAndroidAdb:
    InitAdbTransport(state.get(), options);
    break;
#endif
  default:
    LOG(FATAL) << "Unknown transport: " << options->transport;
  }

  if (!options->suspend) {
    /*
     * Grab a mutex before starting the thread.  This ensures they
     * won't signal the cond var before we're waiting.
     */
    MutexLock thread_start_locker(self, state->thread_start_lock_);
    /*
     * We have bound to a port, or are trying to connect outbound to a
     * debugger.  Create the JDWP thread and let it continue the mission.
     */
    CHECK_PTHREAD_CALL(pthread_create, (&state->pthread_, NULL, StartJdwpThread, state.get()), "JDWP thread");

    /*
     * Wait until the thread finishes basic initialization.
     * TODO: cond vars should be waited upon in a loop
     */
    state->thread_start_cond_.Wait(self);
  } else {
    {
      /*
       * Grab a mutex before starting the thread.  This ensures they
       * won't signal the cond var before we're waiting.
       */
      MutexLock thread_start_locker(self, state->thread_start_lock_);
      /*
       * We have bound to a port, or are trying to connect outbound to a
       * debugger.  Create the JDWP thread and let it continue the mission.
       */
      CHECK_PTHREAD_CALL(pthread_create, (&state->pthread_, NULL, StartJdwpThread, state.get()), "JDWP thread");

      /*
       * Wait until the thread finishes basic initialization.
       * TODO: cond vars should be waited upon in a loop
       */
      state->thread_start_cond_.Wait(self);
    }

    /*
     * For suspend=y, wait for the debugger to connect to us or for us to
     * connect to the debugger.
     *
     * The JDWP thread will signal us when it connects successfully or
     * times out (for timeout=xxx), so we have to check to see what happened
     * when we wake up.
     */
    {
      ScopedThreadStateChange tsc(self, kWaitingForDebuggerToAttach);
      MutexLock attach_locker(self, state->attach_lock_);
      state->attach_cond_.Wait(self);
    }
    if (!state->IsActive()) {
      LOG(ERROR) << "JDWP connection failed";
      return NULL;
    }

    LOG(INFO) << "JDWP connected";

    /*
     * Ordinarily we would pause briefly to allow the debugger to set
     * breakpoints and so on, but for "suspend=y" the VM init code will
     * pause the VM when it sends the VM_START message.
     */
  }

  return state.release();
}

/*
 * Reset all session-related state.  There should not be an active connection
 * to the client at this point.  The rest of the VM still thinks there is
 * a debugger attached.
 *
 * This includes freeing up the debugger event list.
 */
void JdwpState::ResetState() {
  /* could reset the serial numbers, but no need to */

  UnregisterAll();
  {
    MutexLock mu(Thread::Current(), event_list_lock_);
    CHECK(event_list_ == NULL);
  }

  /*
   * Should not have one of these in progress.  If the debugger went away
   * mid-request, though, we could see this.
   */
  if (event_thread_id_ != 0) {
    LOG(WARNING) << "Resetting state while event in progress";
    DCHECK(false);
  }
}

/*
 * Tell the JDWP thread to shut down.  Frees "state".
 */
JdwpState::~JdwpState() {
  if (netState != NULL) {
    if (IsConnected()) {
      PostVMDeath();
    }

    /*
     * Close down the network to inspire the thread to halt.
     */
    VLOG(jdwp) << "JDWP shutting down net...";
    netState->Shutdown();

    if (debug_thread_started_) {
      run = false;
      void* threadReturn;
      if (pthread_join(pthread_, &threadReturn) != 0) {
        LOG(WARNING) << "JDWP thread join failed";
      }
    }

    VLOG(jdwp) << "JDWP freeing netstate...";
    delete netState;
    netState = NULL;
  }
  CHECK(netState == NULL);

  ResetState();
}

/*
 * Are we talking to a debugger?
 */
bool JdwpState::IsActive() {
  return IsConnected();
}

// Returns "false" if we encounter a connection-fatal error.
bool JdwpState::HandlePacket() {
  JdwpNetStateBase* netStateBase = reinterpret_cast<JdwpNetStateBase*>(netState);
  JDWP::Request request(netStateBase->input_buffer_, netStateBase->input_count_);

  ExpandBuf* pReply = expandBufAlloc();
  ProcessRequest(request, pReply);
  ssize_t cc = netStateBase->WritePacket(pReply);
  if (cc != (ssize_t) expandBufGetLength(pReply)) {
    PLOG(ERROR) << "Failed sending reply to debugger";
    expandBufFree(pReply);
    return false;
  }
  expandBufFree(pReply);
  netStateBase->ConsumeBytes(request.GetLength());
  return true;
}

/*
 * Entry point for JDWP thread.  The thread was created through the VM
 * mechanisms, so there is a java/lang/Thread associated with us.
 */
static void* StartJdwpThread(void* arg) {
  JdwpState* state = reinterpret_cast<JdwpState*>(arg);
  CHECK(state != NULL);

  state->Run();
  return NULL;
}

void JdwpState::Run() {
  Runtime* runtime = Runtime::Current();
  CHECK(runtime->AttachCurrentThread("JDWP", true, runtime->GetSystemThreadGroup(),
                                     !runtime->IsCompiler()));

  VLOG(jdwp) << "JDWP: thread running";

  /*
   * Finish initializing, then notify the creating thread that
   * we're running.
   */
  thread_ = Thread::Current();
  run = true;

  {
    MutexLock locker(thread_, thread_start_lock_);
    debug_thread_started_ = true;
    thread_start_cond_.Broadcast(thread_);
  }

  /* set the thread state to kWaitingInMainDebuggerLoop so GCs don't wait for us */
  CHECK_EQ(thread_->GetState(), kNative);
  Locks::mutator_lock_->AssertNotHeld(thread_);
  thread_->SetState(kWaitingInMainDebuggerLoop);

  /*
   * Loop forever if we're in server mode, processing connections.  In
   * non-server mode, we bail out of the thread when the debugger drops
   * us.
   *
   * We broadcast a notification when a debugger attaches, after we
   * successfully process the handshake.
   */
  while (run) {
    if (options_->server) {
      /*
       * Block forever, waiting for a connection.  To support the
       * "timeout=xxx" option we'll need to tweak this.
       */
      if (!netState->Accept()) {
        break;
      }
    } else {
      /*
       * If we're not acting as a server, we need to connect out to the
       * debugger.  To support the "timeout=xxx" option we need to
       * have a timeout if the handshake reply isn't received in a
       * reasonable amount of time.
       */
      if (!netState->Establish(options_)) {
        /* wake anybody who was waiting for us to succeed */
        MutexLock mu(thread_, attach_lock_);
        attach_cond_.Broadcast(thread_);
        break;
      }
    }

    /* prep debug code to handle the new connection */
    Dbg::Connected();

    /* process requests until the debugger drops */
    bool first = true;
    while (!Dbg::IsDisposed()) {
      {
        // sanity check -- shouldn't happen?
        MutexLock mu(thread_, *Locks::thread_suspend_count_lock_);
        CHECK_EQ(thread_->GetState(), kWaitingInMainDebuggerLoop);
      }

      if (!netState->ProcessIncoming()) {
        /* blocking read */
        break;
      }

      if (should_exit_) {
        exit(exit_status_);
      }

      if (first && !netState->IsAwaitingHandshake()) {
        /* handshake worked, tell the interpreter that we're active */
        first = false;

        /* set thread ID; requires object registry to be active */
        {
          ScopedObjectAccess soa(thread_);
          debug_thread_id_ = Dbg::GetThreadSelfId();
        }

        /* wake anybody who's waiting for us */
        MutexLock mu(thread_, attach_lock_);
        attach_cond_.Broadcast(thread_);
      }
    }

    netState->Close();

    if (ddm_is_active_) {
      ddm_is_active_ = false;

      /* broadcast the disconnect; must be in RUNNING state */
      thread_->TransitionFromSuspendedToRunnable();
      Dbg::DdmDisconnected();
      thread_->TransitionFromRunnableToSuspended(kWaitingInMainDebuggerLoop);
    }

    {
      ScopedObjectAccess soa(thread_);

      // Release session state, e.g. remove breakpoint instructions.
      ResetState();
    }
    // Tell the rest of the runtime that the debugger is no longer around.
    Dbg::Disconnected();

    /* if we had threads suspended, resume them now */
    Dbg::UndoDebuggerSuspensions();

    /* if we connected out, this was a one-shot deal */
    if (!options_->server) {
      run = false;
    }
  }

  /* back to native, for thread shutdown */
  CHECK_EQ(thread_->GetState(), kWaitingInMainDebuggerLoop);
  thread_->SetState(kNative);

  VLOG(jdwp) << "JDWP: thread detaching and exiting...";
  runtime->DetachCurrentThread();
}

void JdwpState::NotifyDdmsActive() {
  if (!ddm_is_active_) {
    ddm_is_active_ = true;
    Dbg::DdmConnected();
  }
}

Thread* JdwpState::GetDebugThread() {
  return thread_;
}

/*
 * Support routines for waitForDebugger().
 *
 * We can't have a trivial "waitForDebugger" function that returns the
 * instant the debugger connects, because we run the risk of executing code
 * before the debugger has had a chance to configure breakpoints or issue
 * suspend calls.  It would be nice to just sit in the suspended state, but
 * most debuggers don't expect any threads to be suspended when they attach.
 *
 * There's no JDWP event we can post to tell the debugger, "we've stopped,
 * and we like it that way".  We could send a fake breakpoint, which should
 * cause the debugger to immediately send a resume, but the debugger might
 * send the resume immediately or might throw an exception of its own upon
 * receiving a breakpoint event that it didn't ask for.
 *
 * What we really want is a "wait until the debugger is done configuring
 * stuff" event.  We can approximate this with a "wait until the debugger
 * has been idle for a brief period".
 */

/*
 * Return the time, in milliseconds, since the last debugger activity.
 *
 * Returns -1 if no debugger is attached, or 0 if we're in the middle of
 * processing a debugger request.
 */
int64_t JdwpState::LastDebuggerActivity() {
  if (!Dbg::IsDebuggerActive()) {
    LOG(DEBUG) << "no active debugger";
    return -1;
  }

  int64_t last = QuasiAtomic::Read64(&last_activity_time_ms_);

  /* initializing or in the middle of something? */
  if (last == 0) {
    VLOG(jdwp) << "+++ last=busy";
    return 0;
  }

  /* now get the current time */
  int64_t now = MilliTime();
  CHECK_GE(now, last);

  VLOG(jdwp) << "+++ debugger interval=" << (now - last);
  return now - last;
}

void JdwpState::ExitAfterReplying(int exit_status) {
  LOG(WARNING) << "Debugger told VM to exit with status " << exit_status;
  should_exit_ = true;
  exit_status_ = exit_status;
}

std::ostream& operator<<(std::ostream& os, const JdwpLocation& rhs) {
  os << "JdwpLocation["
     << Dbg::GetClassName(rhs.class_id) << "." << Dbg::GetMethodName(rhs.method_id)
     << "@" << StringPrintf("%#llx", rhs.dex_pc) << " " << rhs.type_tag << "]";
  return os;
}

bool operator==(const JdwpLocation& lhs, const JdwpLocation& rhs) {
  return lhs.dex_pc == rhs.dex_pc && lhs.method_id == rhs.method_id &&
      lhs.class_id == rhs.class_id && lhs.type_tag == rhs.type_tag;
}

bool operator!=(const JdwpLocation& lhs, const JdwpLocation& rhs) {
  return !(lhs == rhs);
}

}  // namespace JDWP

}  // namespace art
