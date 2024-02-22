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

#include "starkware/air/compile_time_optional.h"

#include "gtest/gtest.h"

namespace starkware {
namespace {

template <int N>
struct Foo {
  CompileTimeOptional<uint64_t, (N > 0)> x0 = 10;
  CompileTimeOptional<uint64_t, (N > 1)> x1 = 20;
  CompileTimeOptional<uint64_t, (N > 2)> x2 = 40;
};

TEST(CompileTimeOptional, BasicTest) {
  Foo<2> foo;
  EXPECT_TRUE((std::is_same_v<decltype(foo.x0), uint64_t>));
  EXPECT_TRUE((std::is_same_v<decltype(foo.x1), uint64_t>));
  EXPECT_FALSE((std::is_same_v<decltype(foo.x2), uint64_t>));
  EXPECT_TRUE((std::is_same_v<decltype(foo.x2), HiddenMember<uint64_t>>));

  EXPECT_EQ(ExtractHiddenMemberValue(foo.x0), 10);
  EXPECT_EQ(ExtractHiddenMemberValue(foo.x1), 20);
  EXPECT_EQ(ExtractHiddenMemberValue(foo.x2), 40);
}

}  // namespace
}  // namespace starkware
