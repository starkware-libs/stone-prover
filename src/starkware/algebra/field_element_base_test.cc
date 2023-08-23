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

#include "starkware/algebra/field_element_base.h"

#include <sstream>

#include "gtest/gtest.h"

#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

template <typename T>
class FieldElementBaseTest : public ::testing::Test {};

using TestedFieldTypes =
    ::testing::Types<TestFieldElement, LongFieldElement, PrimeFieldElement<252, 0>>;
TYPED_TEST_CASE(FieldElementBaseTest, TestedFieldTypes);

TYPED_TEST(FieldElementBaseTest, ToString) {
  using FieldElementT = TypeParam;
  Prng prng;
  auto x = FieldElementT::RandomElement(&prng);
  std::stringstream s;
  s << x;
  EXPECT_EQ(x.ToString(), s.str());
}

TYPED_TEST(FieldElementBaseTest, AdditionAssignmentOperator) {
  using FieldElementT = TypeParam;
  Prng prng;
  FieldElementT x = FieldElementT::RandomElement(&prng);
  FieldElementT y = FieldElementT::RandomElement(&prng);

  FieldElementT tmp = x;
  tmp += y;
  EXPECT_EQ(x + y, tmp);
}

TYPED_TEST(FieldElementBaseTest, MultiplicationAssignmentOperator) {
  using FieldElementT = TypeParam;
  Prng prng;
  FieldElementT x = FieldElementT::RandomElement(&prng);
  FieldElementT y = FieldElementT::RandomElement(&prng);

  FieldElementT tmp = x;
  tmp *= y;
  EXPECT_EQ(x * y, tmp);
}

TYPED_TEST(FieldElementBaseTest, Correctness) {
  using FieldElementT = TypeParam;
  EXPECT_TRUE(kIsFieldElement<FieldElementT>);
}

TEST(IsFieldElement, NegativeTest) { EXPECT_FALSE(kIsFieldElement<int>); }

}  // namespace
}  // namespace starkware
