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

#include "starkware/algebra/domains/multiplicative_group.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/math/math.h"

namespace starkware {
namespace {

using testing::HasSubstr;

using TestedFieldTypes =
    ::testing::Types<TestFieldElement, LongFieldElement, PrimeFieldElement<252, 0>>;

template <typename FieldElementT>
class MultiplicativGroupTests : public ::testing::Test {};

TYPED_TEST_CASE(MultiplicativGroupTests, TestedFieldTypes);

TYPED_TEST(MultiplicativGroupTests, SingelElement) {
  using FieldElementT = TypeParam;
  const Field field = Field::Create<FieldElementT>();
  const MultiplicativeGroup group = MultiplicativeGroup::MakeGroup(1U, field);
  EXPECT_EQ(group.Size(), 1U);
  EXPECT_EQ(group.Generator(), FieldElement(FieldElementT::One()));
  EXPECT_EQ(group.ElementByIndex(0), FieldElement(FieldElementT::One()));
  EXPECT_ASSERT(group.ElementByIndex(1), HasSubstr("Index out of range."));
}

TYPED_TEST(MultiplicativGroupTests, TwoElements) {
  using FieldElementT = TypeParam;
  const Field field = Field::Create<FieldElementT>();
  const MultiplicativeGroup group = MultiplicativeGroup::MakeGroup(2U, field);
  EXPECT_EQ(group.Size(), 2U);
  EXPECT_EQ(group.Generator(), FieldElement(-FieldElementT::One()));
  EXPECT_EQ(group.ElementByIndex(0), FieldElement(FieldElementT::One()));
  EXPECT_EQ(group.ElementByIndex(1), FieldElement(-FieldElementT::One()));
  EXPECT_ASSERT(group.ElementByIndex(2), HasSubstr("Index out of range."));
}

TYPED_TEST(MultiplicativGroupTests, Size1024) {
  using FieldElementT = TypeParam;
  const Field field = Field::Create<FieldElementT>();
  const size_t log_group_size = 10;
  const size_t group_size = Pow2(log_group_size);
  const MultiplicativeGroup group = MultiplicativeGroup::MakeGroup(group_size, field);
  EXPECT_EQ(group.Size(), group_size);
  EXPECT_EQ(group.ElementByIndex(0), FieldElement(FieldElementT::One()));
  EXPECT_ASSERT(group.ElementByIndex(group_size), HasSubstr("Index out of range."));
  FieldElement curr_group_element = group.Generator();
  for (size_t i = 1; i < group_size; ++i) {
    ASSERT_EQ(curr_group_element, group.ElementByIndex(i));
    ASSERT_NE(curr_group_element, FieldElement(FieldElementT::One()));
    curr_group_element = curr_group_element * group.Generator();
  }
  EXPECT_EQ(curr_group_element, FieldElement(FieldElementT::One()));
}

void TestGroupWithFactor3(size_t group_size) {
  using FieldElementT = TestFieldElement;
  const Field field = Field::Create<FieldElementT>();
  const MultiplicativeGroup group = MultiplicativeGroup::MakeGroup(group_size, field);
  EXPECT_EQ(group.Size(), group_size);
  EXPECT_EQ(group.ElementByIndex(0), FieldElement(FieldElementT::One()));
  EXPECT_ASSERT(group.ElementByIndex(group_size), HasSubstr("Index out of range."));
  FieldElement curr_group_element = group.Generator();
  for (size_t i = 1; i < group_size; ++i) {
    ASSERT_EQ(curr_group_element, group.ElementByIndex(i));
    ASSERT_NE(curr_group_element, FieldElement(FieldElementT::One()));
    curr_group_element = curr_group_element * group.Generator();
  }
  EXPECT_EQ(curr_group_element, FieldElement(FieldElementT::One()));
}

TEST(MultiplicativGroupTests, Size3) { TestGroupWithFactor3(3); }

TEST(MultiplicativGroupTests, Size6) { TestGroupWithFactor3(6); }

TYPED_TEST(MultiplicativGroupTests, Comparison) {
  using FieldElementT = TypeParam;
  const Field field = Field::Create<FieldElementT>();

  const auto x = MultiplicativeGroup::MakeGroup(4, field);
  const auto y = MultiplicativeGroup::MakeGroup(4, field);
  const auto z = MultiplicativeGroup::MakeGroup(8, field);

  EXPECT_TRUE(x == x);
  EXPECT_TRUE(x == y);
  EXPECT_FALSE(x != x);
  EXPECT_FALSE(y != x);
  EXPECT_FALSE(x == z);
  EXPECT_TRUE(x != z);
}

}  // namespace
}  // namespace starkware
