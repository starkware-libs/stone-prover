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


#include "starkware/utils/profiling.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <utility>

#include "glog/logging.h"

#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/stats.h"

namespace starkware {

namespace {

auto program_start = std::chrono::system_clock::now();

template <typename Duration>
void PrintDuration(std::ostream* os, Duration d) {
  std::chrono::duration<double> sec = d;

  *os << sec.count() << " sec";
}

}  // namespace

ProfilingBlock::ProfilingBlock(std::string description, int k_vlog)
    : start_time_(std::chrono::system_clock::now()),
      description_(std::move(description)),
      k_vlog_(k_vlog) {
  if (FLAGS_v < k_vlog_) {
    return;
  }
  std::stringstream os;

  if (!FLAGS_log_prefix) {
    auto now = std::chrono::system_clock::now();
    PrintDuration(&os, now - program_start);
    os << ": ";
  }
  os << description_ << " started";
  VLOG(k_vlog_) << os.str();
}

ProfilingBlock::~ProfilingBlock() {
  if (!BlockClosed()) {
    CloseBlock();
  }
}

void ProfilingBlock::CloseBlock() {
  if (FLAGS_v < k_vlog_) {
    return;
  }
  ASSERT_RELEASE(!BlockClosed(), "ProfilingBlock.CloseBlock() called twice");

  std::stringstream os;
  auto now = std::chrono::system_clock::now();

  if (!FLAGS_log_prefix) {
    PrintDuration(&os, now - program_start);
    os << ": ";
  }

  os << description_ << " finished in ";

  PrintDuration(&os, now - start_time_);

  VLOG(k_vlog_) << os.str();
  if (FLAGS_v > k_vlog_) {
    std::string stats_to_print = SaveStats(description_);
    VLOG(k_vlog_) << stats_to_print;
  }
  closed_ = true;
}

}  // namespace starkware
