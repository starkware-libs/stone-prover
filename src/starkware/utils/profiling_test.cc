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
#include "starkware/utils/stats.h"

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::HasSubstr;

TEST(Profiling, DoubleClose) {
  FLAGS_v = 1;
  ProfilingBlock profiling_block("test block");
  EXPECT_NO_THROW(profiling_block.CloseBlock());
  EXPECT_ASSERT(
      profiling_block.CloseBlock(), HasSubstr("ProfilingBlock.CloseBlock() called twice"));
  FLAGS_v = 0;
}

}  // namespace
}  // namespace starkware
