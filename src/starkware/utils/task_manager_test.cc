// Copyright 2023 StarkWare Industries Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License").
// You may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.starkware.co/open-source-license/
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions
// and limitations under the License.

#include "starkware/utils/task_manager.h"

#include <numeric>
#include <set>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/cppitertools/range.hpp"

#include "starkware/error_handling/test_utils.h"

namespace starkware {

namespace {

using testing::HasSubstr;

class TaskManagerTest : public ::testing::TestWithParam<size_t> {
 public:
  TaskManagerTest() : manager(TaskManager::CreateInstanceForTesting(GetParam())) {}

  TaskManager manager;
};

// Barrier is used in testing when we want to make sure each worker thread executes at least
// one task.

class Barrier {
 public:
  explicit Barrier(size_t count) : count_(count) {}

  void Wait(std::unique_lock<std::mutex>* lock) {
    --count_;
    while (count_ > 0) {
      cv_.wait(*lock);
    }
    cv_.notify_all();
  }

 private:
  std::condition_variable cv_;
  size_t count_;
};

TEST_P(TaskManagerTest, ParallelFor) {
  uint64_t sum = 0;

  std::vector<uint64_t> v(100);
  std::generate(v.begin(), v.end(), std::rand);

  std::mutex m;
  this->manager.ParallelFor(v.size(), [&](const TaskInfo& task_info) {
    std::unique_lock<std::mutex> l(m);
    sum += v[task_info.start_idx];
  });
  EXPECT_EQ(std::accumulate(v.begin(), v.end(), UINT64_C(0)), sum);
}

void ThrowException(const TaskInfo& /*unused*/) { ASSERT_RELEASE(false, "Exception test."); }

TEST_P(TaskManagerTest, Exception) {
  EXPECT_ASSERT((this->manager.ParallelFor(1U, ThrowException)), HasSubstr("Exception test."));
}

TEST_P(TaskManagerTest, NestedException) {
  TaskManager& manager = this->manager;

  EXPECT_ASSERT(
      manager.ParallelFor(
          2U, [&](const TaskInfo& /*unused*/) { manager.ParallelFor(2U, ThrowException); }),
      HasSubstr("Exception test."));
}

TEST_P(TaskManagerTest, ThreadIds) {
  std::set<std::thread::id> ids = {std::this_thread::get_id()};
  size_t max_thread_count = GetParam();
  std::mutex m;
  Barrier barrier(max_thread_count);

  this->manager.ParallelFor(max_thread_count, [&m, &barrier, &ids](const TaskInfo& /*unused*/) {
    std::unique_lock<std::mutex> l(m);
    ids.insert(std::this_thread::get_id());
    barrier.Wait(&l);
  });

  EXPECT_EQ(ids.size(), max_thread_count);
}

TEST_P(TaskManagerTest, Singleon) {
  std::set<TaskManager*> managers;
  size_t max_thread_count = GetParam();
  std::mutex m;

  this->manager.ParallelFor(4 * max_thread_count, [&m, &managers](const TaskInfo& /*unused*/) {
    TaskManager& manager = TaskManager::GetInstance();
    std::unique_lock<std::mutex> l(m);
    managers.insert(&manager);
  });

  EXPECT_EQ(1U, managers.size());
}

TEST_P(TaskManagerTest, GetNumThreads) { EXPECT_EQ(GetParam(), this->manager.GetNumThreads()); }

TEST_P(TaskManagerTest, WorkerId) {
  std::vector<size_t> ids(this->manager.GetNumThreads());
  std::mutex m;
  Barrier barrier(ids.size());

  this->manager.ParallelFor(ids.size(), [&ids, &barrier, &m](const TaskInfo& /*unused*/) {
    std::unique_lock<std::mutex> l(m);
    size_t worker_id = TaskManager::GetWorkerId();
    ids[worker_id] = worker_id;

    // Block to make sure each worker executes a task.
    barrier.Wait(&l);
  });

  for (size_t i = 0; i < ids.size(); i++) {
    ASSERT_EQ(i, ids[i]);
  }
}

std::set<size_t> n_threads_option = {1, std::thread::hardware_concurrency()};

INSTANTIATE_TEST_CASE_P(
    , TaskManagerTest, ::testing::ValuesIn(n_threads_option), testing::PrintToStringParamName());

}  // namespace

}  // namespace starkware
