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

#ifndef ART_RUNTIME_BASE_TIMING_LOGGER_H_
#define ART_RUNTIME_BASE_TIMING_LOGGER_H_

#include "base/histogram.h"
#include "base/macros.h"
#include "base/mutex.h"

#include <string>
#include <vector>
#include <map>

namespace art {

namespace base {
  class TimingLogger;
}  // namespace base

class CumulativeLogger {
 public:
  explicit CumulativeLogger(const std::string& name);
  void prepare_stats();
  ~CumulativeLogger();
  void Start();
  void End();
  void Reset();
  void Dump(std::ostream& os) LOCKS_EXCLUDED(lock_);
  uint64_t GetTotalNs() const;
  // Allow the name to be modified, particularly when the cumulative logger is a field within a
  // parent class that is unable to determine the "name" of a sub-class.
  void SetName(const std::string& name);
  void AddLogger(const base::TimingLogger& logger) LOCKS_EXCLUDED(lock_);

 private:
  typedef std::map<std::string, Histogram<uint64_t> *> Histograms;
  typedef std::map<std::string, Histogram<uint64_t> *>::const_iterator HistogramsIterator;

  void AddPair(const std::string &label, uint64_t delta_time)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void DumpHistogram(std::ostream &os) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  uint64_t GetTotalTime() const;
  static const uint64_t kAdjust = 1000;
  Histograms histograms_ GUARDED_BY(lock_);
  std::string name_;
  const std::string lock_name_;
  mutable Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  size_t iterations_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(CumulativeLogger);
};

namespace base {


// A timing logger that knows when a split starts for the purposes of logging tools, like systrace.
class TimingLogger {
 public:
  // Splits are nanosecond times and split names.
  typedef std::pair<uint64_t, const char*> SplitTiming;
  typedef std::vector<SplitTiming> SplitTimings;
  typedef std::vector<SplitTiming>::const_iterator SplitTimingsIterator;

  explicit TimingLogger(const char* name, bool precise, bool verbose);

  // Clears current splits and labels.
  void Reset();

  // Starts a split
  void StartSplit(const char* new_split_label);

  // Ends the current split and starts the one given by the label.
  void NewSplit(const char* new_split_label);

  // Ends the current split and records the end time.
  void EndSplit();

  uint64_t GetTotalNs() const;

  void Dump(std::ostream& os) const;

  // Scoped timing splits that can be nested and composed with the explicit split
  // starts and ends.
  class ScopedSplit {
    public:
      explicit ScopedSplit(const char* label, TimingLogger* timing_logger);

      ~ScopedSplit();

      friend class TimingLogger;

    private:
      // Pauses timing of the split, usually due to nesting of another split.
      void Pause();

      // Resumes timing of the split, usually because a nested split has ended.
      void Resume();

      // Used by new split to swap splits in place in a ScopedSplit instance.
      void TailInsertSplit(const char* label);

      // The scoped split immediately enclosing this split. Essentially, we get a
      // stack of nested splits through this field.
      ScopedSplit* enclosing_split_;

      // Was this created via TimingLogger's StartSplit?
      bool explicit_;

      // The split's name.
      const char* label_;

      // The current split's latest start time. (It may have been paused and restarted.)
      uint64_t start_ns_;

      // The running time, outside of pauses.
      uint64_t running_ns_;

      // The timing logger holding this split.
      TimingLogger* timing_logger_;

      DISALLOW_COPY_AND_ASSIGN(ScopedSplit);
  };

  const SplitTimings& GetSplits() const {
    return splits_;
  }

  friend class ScopedSplit;
 protected:
  // The name of the timing logger.
  const char* name_;

  // Do we want to print the exactly recorded split (true) or round down to the time unit being
  // used (false).
  const bool precise_;

  // Verbose logging.
  const bool verbose_;

  // The current scoped split is also the 'top' of the stack of splits in progress.
  ScopedSplit* current_split_;

  // Splits that have ended.
  SplitTimings splits_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimingLogger);
};

}  // namespace base
}  // namespace art

#endif  // ART_RUNTIME_BASE_TIMING_LOGGER_H_
