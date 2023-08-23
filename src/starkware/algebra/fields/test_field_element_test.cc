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

#include "starkware/algebra/fields/test_field_element.h"

#include "gtest/gtest.h"

namespace starkware {
namespace {

TEST(TestFieldElement, Basic) {
  TestFieldElement a = TestFieldElement::FromUint(5), b = TestFieldElement::FromBigInt(0x6_Z);
  EXPECT_EQ(TestFieldElement::FromUint(TestFieldElement::kModulus - 1), a - b);
  EXPECT_EQ(TestFieldElement::TestFieldElement::FromUint(11), a + b);
}

TEST(TestFieldElement, Inverse) {
  TestFieldElement a = TestFieldElement::FromUint(5), b = TestFieldElement::FromUint(6),
                   c = TestFieldElement::FromUint(1);
  TestFieldElement a_inv = a.Inverse();
  TestFieldElement b_inv = b.Inverse();
  TestFieldElement c_inv = c.Inverse();
  EXPECT_EQ(TestFieldElement::One(), a_inv * a);
  EXPECT_EQ(TestFieldElement::One(), b * b_inv);
  EXPECT_EQ(c, c_inv);
}

TEST(TestFieldElement, Division) {
  TestFieldElement a = TestFieldElement::FromUint(5), b = TestFieldElement::FromUint(6),
                   c = TestFieldElement::FromUint(10), d = TestFieldElement::FromUint(12);
  TestFieldElement a_div_b = a / b;
  TestFieldElement c_div_d = c / d;
  EXPECT_EQ(a_div_b, c_div_d);
  EXPECT_EQ(a, a_div_b * b);
}

TEST(TestFieldElement, UnaryMinus) {
  EXPECT_EQ(TestFieldElement::Zero(), -TestFieldElement::Zero());
  Prng prng;
  for (size_t i = 0; i < 100; ++i) {
    TestFieldElement x =
        TestFieldElement::FromUint(prng.UniformInt<uint32_t>(1, (TestFieldElement::kModulus - 1)));
    ASSERT_NE(x, -x);
    ASSERT_EQ(TestFieldElement::Zero(), -x + x);
  }
}

TEST(TestFieldElement, ConstexprFromInt) {
  static constexpr TestFieldElement kOne = TestFieldElement::One();

  static constexpr TestFieldElement kOneFromUint = TestFieldElement::FromUint(1);
  EXPECT_EQ(kOneFromUint, kOne);
  static constexpr TestFieldElement kOneFromBigInt = TestFieldElement::FromBigInt(BigInt<1>::One());
  EXPECT_EQ(kOneFromBigInt, kOne);
}

}  // namespace
}  // namespace starkware
