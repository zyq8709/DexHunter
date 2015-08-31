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


#define ATRACE_TAG ATRACE_TAG_DALVIK
#include <stdio.h>
#include <cutils/trace.h>

#include "timing_logger.h"

#include "base/logging.h"
#include "thread.h"
#include "base/stl_util.h"
#include "base/histogram-inl.h"

#include <cmath>
#include <iomanip>

namespace art {

CumulativeLogger::CumulativeLogger(const std::string& name)
    : name_(name),
      lock_name_("CumulativeLoggerLock" + name),
      lock_(lock_name_.c_str(), kDefaultMutexLevel, true) {
  Reset();
}

CumulativeLogger::~CumulativeLogger() {
  STLDeleteValues(&histograms_);
}

void CumulativeLogger::SetName(const std::string& name) {
  name_.assign(name);
}

void CumulativeLogger::Start() {
}

void CumulativeLogger::End() {
  MutexLock mu(Thread::Current(), lock_);
  iterations_++;
}

void CumulativeLogger::Reset() {
  MutexLock mu(Thread::Current(), lock_);
  iterations_ = 0;
  STLDeleteValues(&histograms_);
}

uint64_t CumulativeLogger::GetTotalNs() const {
  return GetTotalTime() * kAdjust;
}

uint64_t CumulativeLogger::GetTotalTime() const {
  MutexLock mu(Thread::Current(), lock_);
  uint64_t total = 0;
  for (CumulativeLogger::HistogramsIterator it = histograms_.begin(), end = histograms_.end();
       it != end; ++it) {
    total += it->second->Sum();
  }
  return total;
}

void CumulativeLogger::AddLogger(const base::TimingLogger &logger) {
  MutexLock mu(Thread::Current(), lock_);
  const base::TimingLogger::SplitTimings& splits = logger.GetSplits();
  for (base::TimingLogger::SplitTimingsIterator it = splits.begin(), end = splits.end();
       it != end; ++it) {
    base::TimingLogger::SplitTiming split = *it;
    uint64_t split_time = split.first;
    const char* split_name = split.second;
    AddPair(split_name, split_time);
  }
}

void CumulativeLogger::Dump(std::ostream &os) {
  MutexLock mu(Thread::Current(), lock_);
  DumpHistogram(os);
}

void CumulativeLogger::AddPair(const std::string &label, uint64_t delta_time) {
  // Convert delta time to microseconds so that we don't overflow our counters.
  delta_time /= kAdjust;

  if (histograms_.find(label) == histograms_.end()) {
    // TODO: Shoud this be a defined constant so we we know out of which orifice 16 and 100 were picked?
    const size_t max_buckets = Runtime::Current()->GetHeap()->IsLowMemoryMode() ? 16 : 100;
    // TODO: Should this be a defined constant so we know 50 of WTF?
    histograms_[label] = new Histogram<uint64_t>(label.c_str(), 50, max_buckets);
  }
  histograms_[label]->AddValue(delta_time);
}

void CumulativeLogger::DumpHistogram(std::ostream &os) {
  os << "Start Dumping histograms for " << iterations_ << " iterations"
     << " for " << name_ << "\n";
  for (CumulativeLogger::HistogramsIterator it = histograms_.begin(), end = histograms_.end();
       it != end; ++it) {
    Histogram<uint64_t>::CumulativeData cumulative_data;
    it->second->CreateHistogram(cumulative_data);
    it->second->PrintConfidenceIntervals(os, 0.99, cumulative_data);
    // Reset cumulative values to save memory. We don't expect DumpHistogram to be called often, so
    // it is not performance critical.
  }
  os << "Done Dumping histograms \n";
}


namespace base {

TimingLogger::TimingLogger(const char* name, bool precise, bool verbose)
    : name_(name), precise_(precise), verbose_(verbose), current_split_(NULL) {
}

void TimingLogger::Reset() {
  current_split_ = NULL;
  splits_.clear();
}

void TimingLogger::StartSplit(const char* new_split_label) {
  DCHECK(new_split_label != NULL) << "Starting split (" << new_split_label << ") with null label.";
  TimingLogger::ScopedSplit* explicit_scoped_split = new TimingLogger::ScopedSplit(new_split_label, this);
  explicit_scoped_split->explicit_ = true;
}

void TimingLogger::EndSplit() {
  CHECK(current_split_ != NULL) << "Ending a non-existent split.";
  DCHECK(current_split_->label_ != NULL);
  DCHECK(current_split_->explicit_ == true) << "Explicitly ending scoped split: " << current_split_->label_;

  delete current_split_;
}

// Ends the current split and starts the one given by the label.
void TimingLogger::NewSplit(const char* new_split_label) {
  CHECK(current_split_ != NULL) << "Inserting a new split (" << new_split_label
                                << ") into a non-existent split.";
  DCHECK(new_split_label != NULL) << "New split (" << new_split_label << ") with null label.";

  current_split_->TailInsertSplit(new_split_label);
}

uint64_t TimingLogger::GetTotalNs() const {
  uint64_t total_ns = 0;
  for (base::TimingLogger::SplitTimingsIterator it = splits_.begin(), end = splits_.end();
       it != end; ++it) {
    base::TimingLogger::SplitTiming split = *it;
    total_ns += split.first;
  }
  return total_ns;
}

void TimingLogger::Dump(std::ostream &os) const {
  uint64_t longest_split = 0;
  uint64_t total_ns = 0;
  for (base::TimingLogger::SplitTimingsIterator it = splits_.begin(), end = splits_.end();
       it != end; ++it) {
    base::TimingLogger::SplitTiming split = *it;
    uint64_t split_time = split.first;
    longest_split = std::max(longest_split, split_time);
    total_ns += split_time;
  }
  // Compute which type of unit we will use for printing the timings.
  TimeUnit tu = GetAppropriateTimeUnit(longest_split);
  uint64_t divisor = GetNsToTimeUnitDivisor(tu);
  // Print formatted splits.
  for (base::TimingLogger::SplitTimingsIterator it = splits_.begin(), end = splits_.end();
       it != end; ++it) {
    base::TimingLogger::SplitTiming split = *it;
    uint64_t split_time = split.first;
    if (!precise_ && divisor >= 1000) {
      // Make the fractional part 0.
      split_time -= split_time % (divisor / 1000);
    }
    os << name_ << ": " << std::setw(8) << FormatDuration(split_time, tu) << " "
       << split.second << "\n";
  }
  os << name_ << ": end, " << NsToMs(total_ns) << " ms\n";
}


TimingLogger::ScopedSplit::ScopedSplit(const char* label, TimingLogger* timing_logger) {
  DCHECK(label != NULL) << "New scoped split (" << label << ") with null label.";
  CHECK(timing_logger != NULL) << "New scoped split (" << label << ") without TimingLogger.";
  timing_logger_ = timing_logger;
  label_ = label;
  running_ns_ = 0;
  explicit_ = false;

  // Stash away the current split and pause it.
  enclosing_split_ = timing_logger->current_split_;
  if (enclosing_split_ != NULL) {
    enclosing_split_->Pause();
  }

  timing_logger_->current_split_ = this;

  ATRACE_BEGIN(label_);

  start_ns_ = NanoTime();
  if (timing_logger_->verbose_) {
    LOG(INFO) << "Begin: " << label_;
  }
}

TimingLogger::ScopedSplit::~ScopedSplit() {
  uint64_t current_time = NanoTime();
  uint64_t split_time = current_time - start_ns_;
  running_ns_ += split_time;
  ATRACE_END();

  if (timing_logger_->verbose_) {
    LOG(INFO) << "End: " << label_ << " " << PrettyDuration(split_time);
  }

  // If one or more enclosed explcitly started splits are not terminated we can
  // either fail or "unwind" the stack of splits in the timing logger to 'this'
  // (by deleting the intervening scoped splits). This implements the latter.
  TimingLogger::ScopedSplit* current = timing_logger_->current_split_;
  while ((current != NULL) && (current != this)) {
    delete current;
    current = timing_logger_->current_split_;
  }

  CHECK(current != NULL) << "Missing scoped split (" << this->label_
                           << ") in timing logger (" << timing_logger_->name_ << ").";
  CHECK(timing_logger_->current_split_ == this);

  timing_logger_->splits_.push_back(SplitTiming(running_ns_, label_));

  timing_logger_->current_split_ = enclosing_split_;
  if (enclosing_split_ != NULL) {
    enclosing_split_->Resume();
  }
}


void TimingLogger::ScopedSplit::TailInsertSplit(const char* label) {
  // Sleight of hand here: Rather than embedding a new scoped split, we're updating the current
  // scoped split in place. Basically, it's one way to make explicit and scoped splits compose
  // well while maintaining the current semantics of NewSplit. An alternative is to push a new split
  // since we unwind the stack of scoped splits in the scoped split destructor. However, this implies
  // that the current split is not ended by NewSplit (which calls TailInsertSplit), which would
  // be different from what we had before.

  uint64_t current_time = NanoTime();
  uint64_t split_time = current_time - start_ns_;
  ATRACE_END();
  timing_logger_->splits_.push_back(std::pair<uint64_t, const char*>(split_time, label_));

  if (timing_logger_->verbose_) {
    LOG(INFO) << "End: " << label_ << " " << PrettyDuration(split_time) << "\n"
              << "Begin: " << label;
  }

  label_ = label;
  start_ns_ = current_time;
  running_ns_ = 0;

  ATRACE_BEGIN(label);
}

void TimingLogger::ScopedSplit::Pause() {
  uint64_t current_time = NanoTime();
  uint64_t split_time = current_time - start_ns_;
  running_ns_ += split_time;
  ATRACE_END();
}


void TimingLogger::ScopedSplit::Resume() {
  uint64_t current_time = NanoTime();

  start_ns_ = current_time;
  ATRACE_BEGIN(label_);
}

}  // namespace base
}  // namespace art
