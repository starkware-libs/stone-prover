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



#include <unistd.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "glog/logging.h"

#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/profiling.h"
#include "starkware/utils/stats.h"

namespace starkware {

namespace {

constexpr int kVlog = 2;
auto program_start = std::chrono::system_clock::now();
std::vector<PerformanceStats> stats_vector;

}  // namespace

std::string GetLineToPrint(PerformanceStats stats) {
  std::stringstream os;
  os << stats.name << ", ";
  os << "RM:" << stats.resident_memory_usage_mb << "mb, ";
  os << "AM:" << stats.allocated_memory_usage_mb << "mb, ";
  os << "T:" << stats.duration.count() << "sec";
  os << std::endl;
  return os.str();
}
std::string SaveStats(std::string name) {
  if (FLAGS_v < kVlog) {
    return "";
  }
  auto now = std::chrono::system_clock::now();
  std::fstream f("/proc/self/statm", std::ios_base::in);
  if (!f) {
    LOG(ERROR) << "statm couldn't open";
  }
  std::string str;
  std::getline(f, str);
  std::istringstream iss(str);
  size_t resident_memory_usage_pages, allocated_memory_usage_pages;
  iss >> allocated_memory_usage_pages >> resident_memory_usage_pages;
  PerformanceStats stats{/*duration=*/now - program_start,
                         /*resident_memory_usage_mb=*/resident_memory_usage_pages *
                             sysconf(_SC_PAGE_SIZE) / (1024 * 1024),
                         /*allocated_memory_usage_mb=*/allocated_memory_usage_pages *
                             sysconf(_SC_PAGE_SIZE) / (1024 * 1024),
                         /*name=*/std::move(name)};
  stats_vector.push_back(stats);
  return GetLineToPrint(stats);
}

void WriteStats() {
  if (FLAGS_v < kVlog) {
    return;
  }
  std::stringstream os;

  for (auto& stats : stats_vector) {
    os << GetLineToPrint(stats);
  }
  // if --v>=kVlog, it will print.
  VLOG(kVlog) << os.str();
}

}  // namespace starkware
