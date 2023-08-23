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

#include "starkware/utils/stats.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

TEST(Stats, AttemptToSaveAndPrintStats) {
  SaveStats("This is a block");
  int jump_reserve_size = 100;
  std::vector<int> v;
  v.reserve(jump_reserve_size);
  for (int i = 0; i < 10; ++i) {
    v.reserve(jump_reserve_size * (i + 2));
    v.push_back(i);
    SaveStats(std::string("block") + std::to_string(i));
  }
  for (int i = 10; i < 100; ++i) {
    v.push_back(i);
  }
  SaveStats("Final block");
  WriteStats();
}

}  // namespace
}  // namespace starkware
