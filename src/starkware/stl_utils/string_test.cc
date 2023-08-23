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

#include "starkware/stl_utils/string.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

TEST(StlString, SimpleCase) {
  std::string str = "The world is round";
  EXPECT_THAT(Split(str, ' '), testing::ElementsAre("The", "world", "is", "round"));
}

TEST(StlString, EmptyString) {
  std::string str;
  EXPECT_THAT(Split(str, 'a'), testing::IsEmpty());
}

TEST(StlString, DelimiterNotInString) {
  std::string str = "The world is round";
  EXPECT_THAT(Split(str, '?'), testing::ElementsAre("The world is round"));
}

}  // namespace
