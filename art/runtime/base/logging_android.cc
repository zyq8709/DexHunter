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

#include <unistd.h>

#include <iostream>

#include "base/stringprintf.h"
#include "cutils/log.h"

namespace art {

static const int kLogSeverityToAndroidLogPriority[] = {
  ANDROID_LOG_VERBOSE, ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN,
  ANDROID_LOG_ERROR, ANDROID_LOG_FATAL, ANDROID_LOG_FATAL
};

void LogMessage::LogLine(const LogMessageData& data, const char* message) {
  const char* tag = ProgramInvocationShortName();
  int priority = kLogSeverityToAndroidLogPriority[data.severity];
  if (priority == ANDROID_LOG_FATAL) {
    LOG_PRI(priority, tag, "%s:%d] %s", data.file, data.line_number, message);
  } else {
    LOG_PRI(priority, tag, "%s", message);
  }
}

}  // namespace art
