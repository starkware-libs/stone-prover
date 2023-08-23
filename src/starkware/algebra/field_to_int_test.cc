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

#include "starkware/algebra/field_to_int.h"

#include <vector>

#include "gtest/gtest.h"

#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/math/math.h"

namespace starkware {
namespace {

using testing::HasSubstr;

using FieldElementT = PrimeFieldElement<252, 0>;

TEST(FieldToUint, Correctness) {
  EXPECT_EQ(ToUint64(FieldElementT::FromUint(12345)), 12345);
  EXPECT_EQ(ToUint64(FieldElementT::FromUint(uint64_t(-1))), uint64_t(-1));
  EXPECT_ASSERT(
      ToUint64(Pow(FieldElementT::FromUint(2), 64)), HasSubstr("Field element is out of range"));
  EXPECT_ASSERT(ToUint64(-FieldElementT::One()), HasSubstr("Field element is out of range"));
}

TEST(FieldToInt, Correctness) {
  EXPECT_EQ(ToInt64(FieldElementT::FromUint(12345)), 12345);
  EXPECT_EQ(ToInt64(-FieldElementT::FromUint(12345)), -12345);

  EXPECT_EQ(ToInt64(FieldElementT::FromUint(Pow2(63) - 1)), Pow2(63) - 1);
  EXPECT_ASSERT(
      ToInt64(FieldElementT::FromUint(Pow2(63))), HasSubstr("Field element is out of range"));

  EXPECT_EQ(ToInt64(-FieldElementT::FromUint(Pow2(63))), -Pow2(63));
  EXPECT_ASSERT(
      ToInt64(-FieldElementT::FromUint(Pow2(63) + 1)), HasSubstr("Field element is out of range"));

  EXPECT_ASSERT(
      ToInt64(FieldElementT::FromUint(uint64_t(-1))), HasSubstr("Field element is out of range"));
}

}  // namespace
}  // namespace starkware
