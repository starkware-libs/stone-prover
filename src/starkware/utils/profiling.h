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

#ifndef STARKWARE_UTILS_PROFILING_H_
#define STARKWARE_UTILS_PROFILING_H_

#include <chrono>
#include <string>

namespace starkware {

/*
  This class is used to annotate different stages in the prover.
  The class can print out the current stage the prover is located in to assist in profiling.

  Pass the cmd line args -v=1 --logtostderr too see the logging.

  The class can be used in scoped RAII-style:

  int res;
  {
    ProfilingBlock profiling_block("compute res");
    res = compute_res()
  }

  Or with manual block closing when RAII is inconvenient:

  ProfilingBlock profiling_block("compute res");
  auto res = compute_res();
  profiling_block.CloseBlock();

  DO_SOMTHING(res);

*/
class ProfilingBlock {
 public:
  explicit ProfilingBlock(std::string description, int k_vlog = 1);

  ~ProfilingBlock();

  /*
    Close the ProfilingBlock block early, in case  RAII is inconvenient.
  */
  void CloseBlock();
  ProfilingBlock(const ProfilingBlock&) = delete;
  ProfilingBlock& operator=(const ProfilingBlock&) = delete;
  ProfilingBlock(ProfilingBlock&& other) = delete;
  ProfilingBlock& operator=(ProfilingBlock&& other) = delete;

 private:
  bool BlockClosed() { return closed_; }

  std::chrono::time_point<std::chrono::system_clock> start_time_;
  const std::string description_;
  const int k_vlog_;
  bool closed_ = false;
};

}  // namespace starkware

#endif  // STARKWARE_UTILS_PROFILING_H_
