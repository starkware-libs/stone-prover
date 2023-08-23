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

#include <type_traits>

namespace starkware {

inline void TaskManager::CvWithWaitersCount::Wait(std::unique_lock<std::mutex>* lock) {
  // Note that n_sleeping_threads_ is protected by lock.
  ++n_sleeping_threads_;
  cv_.wait(*lock);
  --n_sleeping_threads_;
}

inline bool TaskManager::CvWithWaitersCount::TryNotify() {
  bool has_waiter = n_sleeping_threads_ > 0;
  if (has_waiter) {
    cv_.notify_one();
  }
  return has_waiter;
}

inline void TaskManager::CvWithWaitersCount::NotifyAll() { cv_.notify_all(); }

}  // namespace starkware
