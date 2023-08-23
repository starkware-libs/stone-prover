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

#ifndef STARKWARE_UTILS_STATS_H_
#define STARKWARE_UTILS_STATS_H_

#include <chrono>
#include <string>

namespace starkware {

struct PerformanceStats {
  std::chrono::duration<double> duration;
  size_t resident_memory_usage_mb;
  size_t allocated_memory_usage_mb;
  std::string name;
};

std::string SaveStats(std::string name);
void WriteStats();

}  // namespace starkware

#endif  // STARKWARE_UTILS_STATS_H_
