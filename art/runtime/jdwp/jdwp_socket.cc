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

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "jdwp/jdwp_priv.h"

#define kBasePort           8000
#define kMaxPort            8040

namespace art {

namespace JDWP {

/*
 * JDWP network state.
 *
 * We only talk to one debugger at a time.
 */
struct JdwpSocketState : public JdwpNetStateBase {
  uint16_t listenPort;
  int     listenSock;         /* listen for connection from debugger */

  explicit JdwpSocketState(JdwpState* state) : JdwpNetStateBase(state) {
    listenPort  = 0;
    listenSock  = -1;
  }

  virtual bool Accept();
  virtual bool Establish(const JdwpOptions*);
  virtual void Shutdown();
  virtual bool ProcessIncoming();

 private:
  in_addr remote_addr_;
  uint16_t remote_port_;
};

static JdwpSocketState* SocketStartup(JdwpState* state, uint16_t port, bool probe);

/*
 * Set up some stuff for transport=dt_socket.
 */
bool InitSocketTransport(JdwpState* state, const JdwpOptions* options) {
  uint16_t port = options->port;

  if (options->server) {
    if (options->port != 0) {
      /* try only the specified port */
      state->netState = SocketStartup(state, port, false);
    } else {
      /* scan through a range of ports, binding to the first available */
      for (port = kBasePort; port <= kMaxPort; port++) {
        state->netState = SocketStartup(state, port, true);
        if (state->netState != NULL) {
          break;
        }
      }
    }
    if (state->netState == NULL) {
      LOG(ERROR) << "JDWP net startup failed (req port=" << options->port << ")";
      return false;
    }
  } else {
    state->netState = SocketStartup(state, 0, false);
  }

  if (options->suspend) {
    LOG(INFO) << "JDWP will wait for debugger on port " << port;
  } else {
    LOG(INFO) << "JDWP will " << (options->server ? "listen" : "connect") << " on port " << port;
  }

  return true;
}

/*
 * Initialize JDWP stuff.
 *
 * Allocates a new state structure.  If "port" is non-zero, this also
 * tries to bind to a listen port.  If "port" is zero, we assume
 * we're preparing for an outbound connection, and return without binding
 * to anything.
 *
 * This may be called several times if we're probing for a port.
 *
 * Returns 0 on success.
 */
static JdwpSocketState* SocketStartup(JdwpState* state, uint16_t port, bool probe) {
  JdwpSocketState* netState = new JdwpSocketState(state);
  if (port == 0) {
    return netState;
  }

  netState->listenSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (netState->listenSock < 0) {
    PLOG(probe ? ERROR : FATAL) << "Socket create failed";
    goto fail;
  }

  /* allow immediate re-use */
  {
    int one = 1;
    if (setsockopt(netState->listenSock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
      PLOG(probe ? ERROR : FATAL) << "setsockopt(SO_REUSEADDR) failed";
      goto fail;
    }
  }

  union {
    sockaddr_in  addrInet;
    sockaddr     addrPlain;
  } addr;
  addr.addrInet.sin_family = AF_INET;
  addr.addrInet.sin_port = htons(port);
  inet_aton("127.0.0.1", &addr.addrInet.sin_addr);

  if (bind(netState->listenSock, &addr.addrPlain, sizeof(addr)) != 0) {
    PLOG(probe ? ERROR : FATAL) << "Attempt to bind to port " << port << " failed";
    goto fail;
  }

  netState->listenPort = port;

  if (listen(netState->listenSock, 5) != 0) {
    PLOG(probe ? ERROR : FATAL) << "Listen failed";
    goto fail;
  }

  return netState;

 fail:
  netState->Shutdown();
  delete netState;
  return NULL;
}

/*
 * Shut down JDWP listener.  Don't free state.
 *
 * This may be called from a non-JDWP thread as part of shutting the
 * JDWP thread down.
 *
 * (This is currently called several times during startup as we probe
 * for an open port.)
 */
void JdwpSocketState::Shutdown() {
  int listenSock = this->listenSock;
  int clientSock = this->clientSock;

  /* clear these out so it doesn't wake up and try to reuse them */
  this->listenSock = this->clientSock = -1;

  /* "shutdown" dislodges blocking read() and accept() calls */
  if (listenSock != -1) {
    shutdown(listenSock, SHUT_RDWR);
    close(listenSock);
  }
  if (clientSock != -1) {
    shutdown(clientSock, SHUT_RDWR);
    close(clientSock);
  }

  WakePipe();
}

/*
 * Disable the TCP Nagle algorithm, which delays transmission of outbound
 * packets until the previous transmissions have been acked.  JDWP does a
 * lot of back-and-forth with small packets, so this may help.
 */
static int SetNoDelay(int fd) {
  int on = 1;
  int cc = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  CHECK_EQ(cc, 0);
  return cc;
}

/*
 * Accept a connection.  This will block waiting for somebody to show up.
 * If that's not desirable, use checkConnection() to make sure something
 * is pending.
 */
bool JdwpSocketState::Accept() {
  union {
    sockaddr_in  addrInet;
    sockaddr     addrPlain;
  } addr;
  socklen_t addrlen;
  int sock;

  if (listenSock < 0) {
    return false;       /* you're not listening! */
  }

  CHECK_EQ(clientSock, -1);      /* must not already be talking */

  addrlen = sizeof(addr);
  do {
    sock = accept(listenSock, &addr.addrPlain, &addrlen);
    if (sock < 0 && errno != EINTR) {
      // When we call shutdown() on the socket, accept() returns with
      // EINVAL.  Don't gripe about it.
      if (errno == EINVAL) {
        if (VLOG_IS_ON(jdwp)) {
          PLOG(ERROR) << "accept failed";
        }
      } else {
        PLOG(ERROR) << "accept failed";
        return false;
      }
    }
  } while (sock < 0);

  remote_addr_ = addr.addrInet.sin_addr;
  remote_port_ = ntohs(addr.addrInet.sin_port);
  VLOG(jdwp) << "+++ accepted connection from " << inet_ntoa(remote_addr_) << ":" << remote_port_;

  clientSock = sock;
  SetAwaitingHandshake(true);
  input_count_ = 0;

  VLOG(jdwp) << "Setting TCP_NODELAY on accepted socket";
  SetNoDelay(clientSock);

  if (!MakePipe()) {
    return false;
  }

  return true;
}

/*
 * Create a connection to a waiting debugger.
 */
bool JdwpSocketState::Establish(const JdwpOptions* options) {
  union {
    sockaddr_in  addrInet;
    sockaddr     addrPlain;
  } addr;
  hostent* pEntry;

  CHECK(!options->server);
  CHECK(!options->host.empty());
  CHECK_NE(options->port, 0);

  /*
   * Start by resolving the host name.
   */
#ifdef HAVE_GETHOSTBYNAME_R
  hostent he;
  char auxBuf[128];
  int error;
  int cc = gethostbyname_r(options->host.c_str(), &he, auxBuf, sizeof(auxBuf), &pEntry, &error);
  if (cc != 0) {
    LOG(WARNING) << "gethostbyname_r('" << options->host << "') failed: " << hstrerror(error);
    return false;
  }
#else
  h_errno = 0;
  pEntry = gethostbyname(options->host.c_str());
  if (pEntry == NULL) {
    PLOG(WARNING) << "gethostbyname('" << options->host << "') failed";
    return false;
  }
#endif

  /* copy it out ASAP to minimize risk of multithreaded annoyances */
  memcpy(&addr.addrInet.sin_addr, pEntry->h_addr, pEntry->h_length);
  addr.addrInet.sin_family = pEntry->h_addrtype;

  addr.addrInet.sin_port = htons(options->port);

  LOG(INFO) << "Connecting out to " << inet_ntoa(addr.addrInet.sin_addr) << ":" << ntohs(addr.addrInet.sin_port);

  /*
   * Create a socket.
   */
  clientSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (clientSock < 0) {
    PLOG(ERROR) << "Unable to create socket";
    return false;
  }

  /*
   * Try to connect.
   */
  if (connect(clientSock, &addr.addrPlain, sizeof(addr)) != 0) {
    PLOG(ERROR) << "Unable to connect to " << inet_ntoa(addr.addrInet.sin_addr) << ":" << ntohs(addr.addrInet.sin_port);
    close(clientSock);
    clientSock = -1;
    return false;
  }

  LOG(INFO) << "Connection established to " << options->host << " (" << inet_ntoa(addr.addrInet.sin_addr) << ":" << ntohs(addr.addrInet.sin_port) << ")";
  SetAwaitingHandshake(true);
  input_count_ = 0;

  SetNoDelay(clientSock);

  if (!MakePipe()) {
    return false;
  }

  return true;
}

/*
 * Process incoming data.  If no data is available, this will block until
 * some arrives.
 *
 * If we get a full packet, handle it.
 *
 * To take some of the mystery out of life, we want to reject incoming
 * connections if we already have a debugger attached.  If we don't, the
 * debugger will just mysteriously hang until it times out.  We could just
 * close the listen socket, but there's a good chance we won't be able to
 * bind to the same port again, which would confuse utilities.
 *
 * Returns "false" on error (indicating that the connection has been severed),
 * "true" if things are still okay.
 */
bool JdwpSocketState::ProcessIncoming() {
  int readCount;

  CHECK_NE(clientSock, -1);

  if (!HaveFullPacket()) {
    /* read some more, looping until we have data */
    errno = 0;
    while (1) {
      int selCount;
      fd_set readfds;
      int maxfd = -1;
      int fd;

      FD_ZERO(&readfds);

      /* configure fds; note these may get zapped by another thread */
      fd = listenSock;
      if (fd >= 0) {
        FD_SET(fd, &readfds);
        if (maxfd < fd) {
          maxfd = fd;
        }
      }
      fd = clientSock;
      if (fd >= 0) {
        FD_SET(fd, &readfds);
        if (maxfd < fd) {
          maxfd = fd;
        }
      }
      fd = wake_pipe_[0];
      if (fd >= 0) {
        FD_SET(fd, &readfds);
        if (maxfd < fd) {
          maxfd = fd;
        }
      } else {
        LOG(INFO) << "NOTE: entering select w/o wakepipe";
      }

      if (maxfd < 0) {
        VLOG(jdwp) << "+++ all fds are closed";
        return false;
      }

      /*
       * Select blocks until it sees activity on the file descriptors.
       * Closing the local file descriptor does not count as activity,
       * so we can't rely on that to wake us up (it works for read()
       * and accept(), but not select()).
       *
       * We can do one of three things: (1) send a signal and catch
       * EINTR, (2) open an additional fd ("wake pipe") and write to
       * it when it's time to exit, or (3) time out periodically and
       * re-issue the select.  We're currently using #2, as it's more
       * reliable than #1 and generally better than #3.  Wastes two fds.
       */
      selCount = select(maxfd+1, &readfds, NULL, NULL, NULL);
      if (selCount < 0) {
        if (errno == EINTR) {
          continue;
        }
        PLOG(ERROR) << "select failed";
        goto fail;
      }

      if (wake_pipe_[0] >= 0 && FD_ISSET(wake_pipe_[0], &readfds)) {
        if (listenSock >= 0) {
          LOG(ERROR) << "Exit wake set, but not exiting?";
        } else {
          LOG(DEBUG) << "Got wake-up signal, bailing out of select";
        }
        goto fail;
      }
      if (listenSock >= 0 && FD_ISSET(listenSock, &readfds)) {
        LOG(INFO) << "Ignoring second debugger -- accepting and dropping";
        union {
          sockaddr_in   addrInet;
          sockaddr      addrPlain;
        } addr;
        socklen_t addrlen;
        int tmpSock;
        tmpSock = accept(listenSock, &addr.addrPlain, &addrlen);
        if (tmpSock < 0) {
          LOG(INFO) << "Weird -- accept failed";
        } else {
          close(tmpSock);
        }
      }
      if (clientSock >= 0 && FD_ISSET(clientSock, &readfds)) {
        readCount = read(clientSock, input_buffer_ + input_count_, sizeof(input_buffer_) - input_count_);
        if (readCount < 0) {
          /* read failed */
          if (errno != EINTR) {
            goto fail;
          }
          LOG(DEBUG) << "+++ EINTR hit";
          return true;
        } else if (readCount == 0) {
          /* EOF hit -- far end went away */
          VLOG(jdwp) << "+++ peer disconnected";
          goto fail;
        } else {
          break;
        }
      }
    }

    input_count_ += readCount;
    if (!HaveFullPacket()) {
      return true;        /* still not there yet */
    }
  }

  /*
   * Special-case the initial handshake.  For some bizarre reason we're
   * expected to emulate bad tty settings by echoing the request back
   * exactly as it was sent.  Note the handshake is always initiated by
   * the debugger, no matter who connects to whom.
   *
   * Other than this one case, the protocol [claims to be] stateless.
   */
  if (IsAwaitingHandshake()) {
    if (memcmp(input_buffer_, kMagicHandshake, kMagicHandshakeLen) != 0) {
      LOG(ERROR) << StringPrintf("ERROR: bad handshake '%.14s'", input_buffer_);
      goto fail;
    }

    errno = 0;
    int cc = TEMP_FAILURE_RETRY(write(clientSock, input_buffer_, kMagicHandshakeLen));
    if (cc != kMagicHandshakeLen) {
      PLOG(ERROR) << "Failed writing handshake bytes (" << cc << " of " << kMagicHandshakeLen << ")";
      goto fail;
    }

    ConsumeBytes(kMagicHandshakeLen);
    SetAwaitingHandshake(false);
    VLOG(jdwp) << "+++ handshake complete";
    return true;
  }

  /*
   * Handle this packet.
   */
  return state_->HandlePacket();

 fail:
  Close();
  return false;
}

}  // namespace JDWP

}  // namespace art
