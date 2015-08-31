/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "barrier.h"

#include <string>

#include "atomic_integer.h"
#include "common_test.h"
#include "mirror/object_array-inl.h"
#include "thread_pool.h"
#include "UniquePtr.h"

namespace art {
class CheckWaitTask : public Task {
 public:
  CheckWaitTask(Barrier* barrier, AtomicInteger* count1, AtomicInteger* count2,
                   AtomicInteger* count3)
      : barrier_(barrier),
        count1_(count1),
        count2_(count2),
        count3_(count3) {}

  void Run(Thread* self) {
    LOG(INFO) << "Before barrier 1 " << *self;
    ++*count1_;
    barrier_->Wait(self);
    ++*count2_;
    LOG(INFO) << "Before barrier 2 " << *self;
    barrier_->Wait(self);
    ++*count3_;
    LOG(INFO) << "After barrier 2 " << *self;
  }

  virtual void Finalize() {
    delete this;
  }

 private:
  Barrier* const barrier_;
  AtomicInteger* const count1_;
  AtomicInteger* const count2_;
  AtomicInteger* const count3_;
};

class BarrierTest : public CommonTest {
 public:
  static int32_t num_threads;
};

int32_t BarrierTest::num_threads = 4;

// Check that barrier wait and barrier increment work.
TEST_F(BarrierTest, CheckWait) {
  Thread* self = Thread::Current();
  ThreadPool thread_pool(num_threads);
  Barrier barrier(0);
  AtomicInteger count1(0);
  AtomicInteger count2(0);
  AtomicInteger count3(0);
  for (int32_t i = 0; i < num_threads; ++i) {
    thread_pool.AddTask(self, new CheckWaitTask(&barrier, &count1, &count2, &count3));
  }
  thread_pool.StartWorkers(self);
  barrier.Increment(self, num_threads);
  // At this point each thread should have passed through the barrier. The first count should be
  // equal to num_threads.
  EXPECT_EQ(num_threads, count1);
  // Count 3 should still be zero since no thread should have gone past the second barrier.
  EXPECT_EQ(0, count3);
  // Now lets tell the threads to pass again.
  barrier.Increment(self, num_threads);
  // Count 2 should be equal to num_threads since each thread must have passed the second barrier
  // at this point.
  EXPECT_EQ(num_threads, count2);
  // Wait for all the threads to finish.
  thread_pool.Wait(self, true, false);
  // All three counts should be equal to num_threads now.
  EXPECT_EQ(count1, count2);
  EXPECT_EQ(count2, count3);
  EXPECT_EQ(num_threads, count3);
}

class CheckPassTask : public Task {
 public:
  CheckPassTask(Barrier* barrier, AtomicInteger* count, size_t subtasks)
      : barrier_(barrier),
        count_(count),
        subtasks_(subtasks) {}

  void Run(Thread* self) {
    for (size_t i = 0; i < subtasks_; ++i) {
      ++*count_;
      // Pass through to next subtask.
      barrier_->Pass(self);
    }
  }

  void Finalize() {
    delete this;
  }
 private:
  Barrier* const barrier_;
  AtomicInteger* const count_;
  const size_t subtasks_;
};

// Check that barrier pass through works.
TEST_F(BarrierTest, CheckPass) {
  Thread* self = Thread::Current();
  ThreadPool thread_pool(num_threads);
  Barrier barrier(0);
  AtomicInteger count(0);
  const int32_t num_tasks = num_threads * 4;
  const int32_t num_sub_tasks = 128;
  for (int32_t i = 0; i < num_tasks; ++i) {
    thread_pool.AddTask(self, new CheckPassTask(&barrier, &count, num_sub_tasks));
  }
  thread_pool.StartWorkers(self);
  const int32_t expected_total_tasks = num_sub_tasks * num_tasks;
  // Wait for all the tasks to complete using the barrier.
  barrier.Increment(self, expected_total_tasks);
  // The total number of completed tasks should be equal to expected_total_tasks.
  EXPECT_EQ(count, expected_total_tasks);
}

}  // namespace art
