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

#ifndef STARKWARE_UTILS_NANOTIMER_H_
#define STARKWARE_UTILS_NANOTIMER_H_

#include <chrono>

namespace starkware {

/*
  Provides only the basic functionality we normally care about in order to measure time.
*/
class NanoTimer {
 public:
  // This is a very thin wrapper for chrono, meant to save using its somewhat
  // inconvenient-for-our-purposes objects.
  NanoTimer() { Reset(); }

  uint64_t Elapsed() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now() - latest)
        .count();
  }

  uint64_t ElapsedSeconds() { return uint64_t(Elapsed() * 1e-9); }

  void Reset() { latest = std::chrono::system_clock::now(); }

  std::chrono::time_point<std::chrono::system_clock> latest;
};

}  // namespace starkware

#endif  // STARKWARE_UTILS_NANOTIMER_H_
