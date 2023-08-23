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

#include "starkware/proof_system/proof_system.h"

#include <stdexcept>

#include "gtest/gtest.h"

#include "starkware/error_handling/error_handling.h"

namespace starkware {
namespace {

TEST(FalseOnError, Correctness) {
  EXPECT_TRUE(FalseOnError([]() { ASSERT_RELEASE(true, ""); }));
  EXPECT_FALSE(FalseOnError([]() { ASSERT_RELEASE(false, ""); }));
  EXPECT_THROW(FalseOnError([]() { throw std::invalid_argument(""); }), std::invalid_argument);
}

}  // namespace
}  // namespace starkware
