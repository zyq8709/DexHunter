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

#include "monitor.h"
#include "thread.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cutils/log.h"

#define EVENT_LOG_TAG_dvm_lock_sample 20003

namespace art {

static void Set4LE(uint8_t* buf, uint32_t val) {
  *buf++ = (uint8_t)(val);
  *buf++ = (uint8_t)(val >> 8);
  *buf++ = (uint8_t)(val >> 16);
  *buf = (uint8_t)(val >> 24);
}

static char* EventLogWriteInt(char* dst, int value) {
  *dst++ = EVENT_TYPE_INT;
  Set4LE(reinterpret_cast<uint8_t*>(dst), value);
  return dst + 4;
}

static char* EventLogWriteString(char* dst, const char* value, size_t len) {
  *dst++ = EVENT_TYPE_STRING;
  len = len < 32 ? len : 32;
  Set4LE(reinterpret_cast<uint8_t*>(dst), len);
  dst += 4;
  memcpy(dst, value, len);
  return dst + len;
}

void Monitor::LogContentionEvent(Thread* self, uint32_t wait_ms, uint32_t sample_percent,
                                 const char* owner_filename, uint32_t owner_line_number) {
  // Emit the event list length, 1 byte.
  char eventBuffer[174];
  char* cp = eventBuffer;
  *cp++ = 9;

  // Emit the process name, <= 37 bytes.
  int fd = open("/proc/self/cmdline", O_RDONLY);
  char procName[33];
  memset(procName, 0, sizeof(procName));
  read(fd, procName, sizeof(procName) - 1);
  close(fd);
  size_t len = strlen(procName);
  cp = EventLogWriteString(cp, procName, len);

  // Emit the sensitive thread ("main thread") status, 5 bytes.
  cp = EventLogWriteInt(cp, Monitor::IsSensitiveThread());

  // Emit self thread name string, <= 37 bytes.
  std::string thread_name;
  self->GetThreadName(thread_name);
  cp = EventLogWriteString(cp, thread_name.c_str(), thread_name.size());

  // Emit the wait time, 5 bytes.
  cp = EventLogWriteInt(cp, wait_ms);

  // Emit the source code file name, <= 37 bytes.
  uint32_t pc;
  mirror::ArtMethod* m = self->GetCurrentMethod(&pc);
  const char* filename;
  uint32_t line_number;
  TranslateLocation(m, pc, filename, line_number);
  cp = EventLogWriteString(cp, filename, strlen(filename));

  // Emit the source code line number, 5 bytes.
  cp = EventLogWriteInt(cp, line_number);

  // Emit the lock owner source code file name, <= 37 bytes.
  if (owner_filename == NULL) {
    owner_filename = "";
  } else if (strcmp(filename, owner_filename) == 0) {
    // Common case, so save on log space.
    owner_filename = "-";
  }
  cp = EventLogWriteString(cp, owner_filename, strlen(owner_filename));

  // Emit the source code line number, 5 bytes.
  cp = EventLogWriteInt(cp, owner_line_number);

  // Emit the sample percentage, 5 bytes.
  cp = EventLogWriteInt(cp, sample_percent);

  CHECK_LE((size_t)(cp - eventBuffer), sizeof(eventBuffer));
  android_btWriteLog(EVENT_LOG_TAG_dvm_lock_sample, EVENT_TYPE_LIST, eventBuffer,
                     (size_t)(cp - eventBuffer));
}

}  // namespace art
