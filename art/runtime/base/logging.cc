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

#include "logging.h"

#include "base/mutex.h"
#include "runtime.h"
#include "thread.h"
#include "utils.h"

namespace art {

LogVerbosity gLogVerbosity;

unsigned int gAborting = 0;

static LogSeverity gMinimumLogSeverity = INFO;
static std::string* gCmdLine = NULL;
static std::string* gProgramInvocationName = NULL;
static std::string* gProgramInvocationShortName = NULL;

const char* GetCmdLine() {
  return (gCmdLine != NULL) ? gCmdLine->c_str() : NULL;
}

const char* ProgramInvocationName() {
  return (gProgramInvocationName != NULL) ? gProgramInvocationName->c_str() : "art";
}

const char* ProgramInvocationShortName() {
  return (gProgramInvocationShortName != NULL) ? gProgramInvocationShortName->c_str() : "art";
}

// Configure logging based on ANDROID_LOG_TAGS environment variable.
// We need to parse a string that looks like
//
//      *:v jdwp:d dalvikvm:d dalvikvm-gc:i dalvikvmi:i
//
// The tag (or '*' for the global level) comes first, followed by a colon
// and a letter indicating the minimum priority level we're expected to log.
// This can be used to reveal or conceal logs with specific tags.
void InitLogging(char* argv[]) {
  if (gCmdLine != NULL) {
    return;
  }
  // TODO: Move this to a more obvious InitART...
  Locks::Init();

  // Stash the command line for later use. We can use /proc/self/cmdline on Linux to recover this,
  // but we don't have that luxury on the Mac, and there are a couple of argv[0] variants that are
  // commonly used.
  if (argv != NULL) {
    gCmdLine = new std::string(argv[0]);
    for (size_t i = 1; argv[i] != NULL; ++i) {
      gCmdLine->append(" ");
      gCmdLine->append(argv[i]);
    }
    gProgramInvocationName = new std::string(argv[0]);
    const char* last_slash = strrchr(argv[0], '/');
    gProgramInvocationShortName = new std::string((last_slash != NULL) ? last_slash + 1 : argv[0]);
  } else {
    // TODO: fall back to /proc/self/cmdline when argv is NULL on Linux
    gCmdLine = new std::string("<unset>");
  }
  const char* tags = getenv("ANDROID_LOG_TAGS");
  if (tags == NULL) {
    return;
  }

  std::vector<std::string> specs;
  Split(tags, ' ', specs);
  for (size_t i = 0; i < specs.size(); ++i) {
    // "tag-pattern:[vdiwefs]"
    std::string spec(specs[i]);
    if (spec.size() == 3 && StartsWith(spec, "*:")) {
      switch (spec[2]) {
        case 'v':
          gMinimumLogSeverity = VERBOSE;
          continue;
        case 'd':
          gMinimumLogSeverity = DEBUG;
          continue;
        case 'i':
          gMinimumLogSeverity = INFO;
          continue;
        case 'w':
          gMinimumLogSeverity = WARNING;
          continue;
        case 'e':
          gMinimumLogSeverity = ERROR;
          continue;
        case 'f':
          gMinimumLogSeverity = FATAL;
          continue;
        // liblog will even suppress FATAL if you say 's' for silent, but that's crazy!
        case 's':
          gMinimumLogSeverity = FATAL;
          continue;
      }
    }
    LOG(FATAL) << "unsupported '" << spec << "' in ANDROID_LOG_TAGS (" << tags << ")";
  }
}

LogMessageData::LogMessageData(const char* file, int line, LogSeverity severity, int error)
    : file(file),
      line_number(line),
      severity(severity),
      error(error) {
  const char* last_slash = strrchr(file, '/');
  file = (last_slash == NULL) ? file : last_slash + 1;
}

LogMessage::~LogMessage() {
  if (data_->severity < gMinimumLogSeverity) {
    return;  // No need to format something we're not going to output.
  }

  // Finish constructing the message.
  if (data_->error != -1) {
    data_->buffer << ": " << strerror(data_->error);
  }
  std::string msg(data_->buffer.str());

  // Do the actual logging with the lock held.
  {
    MutexLock mu(Thread::Current(), *Locks::logging_lock_);
    if (msg.find('\n') == std::string::npos) {
      LogLine(*data_, msg.c_str());
    } else {
      msg += '\n';
      size_t i = 0;
      while (i < msg.size()) {
        size_t nl = msg.find('\n', i);
        msg[nl] = '\0';
        LogLine(*data_, &msg[i]);
        i = nl + 1;
      }
    }
  }

  // Abort if necessary.
  if (data_->severity == FATAL) {
    Runtime::Abort();
  }
}

HexDump::HexDump(const void* address, size_t byte_count, bool show_actual_addresses)
    : address_(address), byte_count_(byte_count), show_actual_addresses_(show_actual_addresses) {
}

void HexDump::Dump(std::ostream& os) const {
  if (byte_count_ == 0) {
    return;
  }

  if (address_ == NULL) {
    os << "00000000:";
    return;
  }

  static const char gHexDigit[] = "0123456789abcdef";
  const unsigned char* addr = reinterpret_cast<const unsigned char*>(address_);
  char out[76];           /* exact fit */
  unsigned int offset;    /* offset to show while printing */

  if (show_actual_addresses_) {
    offset = reinterpret_cast<int>(addr);
  } else {
    offset = 0;
  }
  memset(out, ' ', sizeof(out)-1);
  out[8] = ':';
  out[sizeof(out)-1] = '\0';

  size_t byte_count = byte_count_;
  int gap = static_cast<int>(offset & 0x0f);
  while (byte_count) {
    unsigned int line_offset = offset & ~0x0f;

    char* hex = out;
    char* asc = out + 59;

    for (int i = 0; i < 8; i++) {
      *hex++ = gHexDigit[line_offset >> 28];
      line_offset <<= 4;
    }
    hex++;
    hex++;

    int count = std::min(static_cast<int>(byte_count), 16 - gap);
    CHECK_NE(count, 0);
    CHECK_LE(count + gap, 16);

    if (gap) {
      /* only on first line */
      hex += gap * 3;
      asc += gap;
    }

    int i;
    for (i = gap ; i < count+gap; i++) {
      *hex++ = gHexDigit[*addr >> 4];
      *hex++ = gHexDigit[*addr & 0x0f];
      hex++;
      if (*addr >= 0x20 && *addr < 0x7f /*isprint(*addr)*/) {
        *asc++ = *addr;
      } else {
        *asc++ = '.';
      }
      addr++;
    }
    for (; i < 16; i++) {
      /* erase extra stuff; only happens on last line */
      *hex++ = ' ';
      *hex++ = ' ';
      hex++;
      *asc++ = ' ';
    }

    os << out;

    gap = 0;
    byte_count -= count;
    offset += count;
  }
}

std::ostream& operator<<(std::ostream& os, const HexDump& rhs) {
  rhs.Dump(os);
  return os;
}

}  // namespace art
