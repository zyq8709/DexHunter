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

#include "thread.h"

#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>
#include <errno.h>

#include <cutils/sched_policy.h>
#include <utils/threads.h>

#include "base/macros.h"

namespace art {

// Conversion map for "nice" values.
//
// We use Android thread priority constants to be consistent with the rest
// of the system.  In some cases adjacent entries may overlap.
//
static const int kNiceValues[10] = {
  ANDROID_PRIORITY_LOWEST,                // 1 (MIN_PRIORITY)
  ANDROID_PRIORITY_BACKGROUND + 6,
  ANDROID_PRIORITY_BACKGROUND + 3,
  ANDROID_PRIORITY_BACKGROUND,
  ANDROID_PRIORITY_NORMAL,                // 5 (NORM_PRIORITY)
  ANDROID_PRIORITY_NORMAL - 2,
  ANDROID_PRIORITY_NORMAL - 4,
  ANDROID_PRIORITY_URGENT_DISPLAY + 3,
  ANDROID_PRIORITY_URGENT_DISPLAY + 2,
  ANDROID_PRIORITY_URGENT_DISPLAY         // 10 (MAX_PRIORITY)
};

void Thread::SetNativePriority(int newPriority) {
  if (newPriority < 1 || newPriority > 10) {
    LOG(WARNING) << "bad priority " << newPriority;
    newPriority = 5;
  }

  int newNice = kNiceValues[newPriority-1];
  pid_t tid = GetTid();

  if (newNice >= ANDROID_PRIORITY_BACKGROUND) {
    set_sched_policy(tid, SP_BACKGROUND);
  } else if (getpriority(PRIO_PROCESS, tid) >= ANDROID_PRIORITY_BACKGROUND) {
    set_sched_policy(tid, SP_FOREGROUND);
  }

  if (setpriority(PRIO_PROCESS, tid, newNice) != 0) {
    PLOG(INFO) << *this << " setPriority(PRIO_PROCESS, " << tid << ", " << newNice << ") failed";
  }
}

int Thread::GetNativePriority() {
  errno = 0;
  int native_priority = getpriority(PRIO_PROCESS, 0);
  if (native_priority == -1 && errno != 0) {
    PLOG(WARNING) << "getpriority failed";
    return kNormThreadPriority;
  }

  int managed_priority = kMinThreadPriority;
  for (size_t i = 0; i < arraysize(kNiceValues); i++) {
    if (native_priority >= kNiceValues[i]) {
      break;
    }
    managed_priority++;
  }
  if (managed_priority > kMaxThreadPriority) {
    managed_priority = kMaxThreadPriority;
  }
  return managed_priority;
}

void Thread::SetUpAlternateSignalStack() {
  // Bionic does this for us.
}

void Thread::TearDownAlternateSignalStack() {
  // Bionic does this for us.
}

}  // namespace art
