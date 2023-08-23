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

#include "starkware/algebra/polymorphic/field_element.h"

#include <sstream>

#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

TEST(FieldElement, BasicTest) {
  FieldElement a(TestFieldElement::FromUint(5));
  FieldElement b(TestFieldElement::FromUint(3));
  EXPECT_EQ(FieldElement(TestFieldElement::FromUint(8)), a + b);
  EXPECT_EQ(FieldElement(TestFieldElement::FromUint(15)), a * b);
  EXPECT_EQ(FieldElement(TestFieldElement::FromUint(2)), a - b);
  EXPECT_TRUE(
      FieldElement(TestFieldElement::FromUint(8)) == FieldElement(TestFieldElement::FromUint(8)));
  EXPECT_FALSE(
      FieldElement(TestFieldElement::FromUint(8)) == FieldElement(TestFieldElement::FromUint(10)));
  EXPECT_FALSE(
      FieldElement(TestFieldElement::FromUint(8)) != FieldElement(TestFieldElement::FromUint(8)));
  EXPECT_TRUE(
      FieldElement(TestFieldElement::FromUint(8)) != FieldElement(TestFieldElement::FromUint(10)));
  EXPECT_EQ(
      FieldElement(TestFieldElement::FromUint(5)).Pow(3),
      FieldElement(Pow<TestFieldElement>(TestFieldElement::FromUint(5), 3)));
}

TEST(FieldElement, CopyConstructor) {
  FieldElement a(TestFieldElement::FromUint(5));
  FieldElement b(a);  // NOLINT
  EXPECT_EQ(FieldElement(TestFieldElement::FromUint(5)), b);
}

TEST(FieldElement, AssignOperator) {
  FieldElement a(TestFieldElement::FromUint(5));
  FieldElement b(TestFieldElement::FromUint(3));
  a = b;
  EXPECT_EQ(FieldElement(TestFieldElement::FromUint(3)), a);
  EXPECT_EQ(FieldElement(TestFieldElement::FromUint(3)), b);
}

TEST(FieldElement, SelfAssign) {
  FieldElement a(TestFieldElement::FromUint(5));
  a.operator=(a);  // Call operator= explicitly to avoid self assignment warning.
  EXPECT_EQ(FieldElement(TestFieldElement::FromUint(5)), a);
}

TEST(FieldElement, ToString) {
  Prng prng;
  FieldElement a(TestFieldElement::RandomElement(&prng));
  EXPECT_EQ(a.As<TestFieldElement>().ToString(), a.ToString());

  std::ostringstream s;
  s << a;
  EXPECT_EQ(a.As<TestFieldElement>().ToString(), s.str());
}

}  // namespace
}  // namespace starkware
