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


#include <string>

#include "atomic_integer.h"
#include "common_test.h"
#include "thread_pool.h"

namespace art {

class CountTask : public Task {
 public:
  explicit CountTask(AtomicInteger* count) : count_(count), verbose_(false) {}

  void Run(Thread* self) {
    if (verbose_) {
      LOG(INFO) << "Running: " << *self;
    }
    // Simulate doing some work.
    usleep(100);
    // Increment the counter which keeps track of work completed.
    ++*count_;
  }

  void Finalize() {
    if (verbose_) {
      LOG(INFO) << "Finalizing: " << *Thread::Current();
    }
    delete this;
  }

 private:
  AtomicInteger* const count_;
  const bool verbose_;
};

class ThreadPoolTest : public CommonTest {
 public:
  static int32_t num_threads;
};

int32_t ThreadPoolTest::num_threads = 4;

// Check that the thread pool actually runs tasks that you assign it.
TEST_F(ThreadPoolTest, CheckRun) {
  Thread* self = Thread::Current();
  ThreadPool thread_pool(num_threads);
  AtomicInteger count(0);
  static const int32_t num_tasks = num_threads * 4;
  for (int32_t i = 0; i < num_tasks; ++i) {
    thread_pool.AddTask(self, new CountTask(&count));
  }
  thread_pool.StartWorkers(self);
  // Wait for tasks to complete.
  thread_pool.Wait(self, true, false);
  // Make sure that we finished all the work.
  EXPECT_EQ(num_tasks, count);
}

TEST_F(ThreadPoolTest, StopStart) {
  Thread* self = Thread::Current();
  ThreadPool thread_pool(num_threads);
  AtomicInteger count(0);
  static const int32_t num_tasks = num_threads * 4;
  for (int32_t i = 0; i < num_tasks; ++i) {
    thread_pool.AddTask(self, new CountTask(&count));
  }
  usleep(200);
  // Check that no threads started prematurely.
  EXPECT_EQ(0, count);
  // Signal the threads to start processing tasks.
  thread_pool.StartWorkers(self);
  usleep(200);
  thread_pool.StopWorkers(self);
  AtomicInteger bad_count(0);
  thread_pool.AddTask(self, new CountTask(&bad_count));
  usleep(200);
  // Ensure that the task added after the workers were stopped doesn't get run.
  EXPECT_EQ(0, bad_count);
  // Allow tasks to finish up and delete themselves.
  thread_pool.StartWorkers(self);
  while (count.load() != num_tasks && bad_count.load() != 1) {
    usleep(200);
  }
  thread_pool.StopWorkers(self);
}

class TreeTask : public Task {
 public:
  TreeTask(ThreadPool* const thread_pool, AtomicInteger* count, int depth)
      : thread_pool_(thread_pool),
        count_(count),
        depth_(depth) {}

  void Run(Thread* self) {
    if (depth_ > 1) {
      thread_pool_->AddTask(self, new TreeTask(thread_pool_, count_, depth_ - 1));
      thread_pool_->AddTask(self, new TreeTask(thread_pool_, count_, depth_ - 1));
    }
    // Increment the counter which keeps track of work completed.
    ++*count_;
  }

  void Finalize() {
    delete this;
  }

 private:
  ThreadPool* const thread_pool_;
  AtomicInteger* const count_;
  const int depth_;
};

// Test that adding new tasks from within a task works.
TEST_F(ThreadPoolTest, RecursiveTest) {
  Thread* self = Thread::Current();
  ThreadPool thread_pool(num_threads);
  AtomicInteger count(0);
  static const int depth = 8;
  thread_pool.AddTask(self, new TreeTask(&thread_pool, &count, depth));
  thread_pool.StartWorkers(self);
  thread_pool.Wait(self, true, false);
  EXPECT_EQ((1 << depth) - 1, count);
}

}  // namespace art
