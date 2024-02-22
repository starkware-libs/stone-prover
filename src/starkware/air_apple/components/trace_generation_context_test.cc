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

#include "starkware/air/components/trace_generation_context.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::HasSubstr;

TEST(TraceGenerationContext, Objects) {
  TraceGenerationContext ctx;
  ctx.AddObject<uint16_t>("uint16", 10);
  ASSERT_EQ(ctx.GetObject<uint16_t>("uint16"), 10);
  EXPECT_ASSERT(ctx.GetObject<uint32_t>("uint16"), HasSubstr("is of the wrong type"));
  EXPECT_ASSERT(ctx.GetObject<uint8_t>("uint16"), HasSubstr("is of the wrong type"));

  EXPECT_ASSERT(ctx.GetObject<uint32_t>("not_exists"), HasSubstr("not found"));
}

}  // namespace
}  // namespace starkware
