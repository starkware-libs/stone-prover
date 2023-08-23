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

#include "starkware/algebra/fields/extension_field_element.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using ExtensionFieldElementT = ExtensionFieldElement<TestFieldElement>;

ExtensionFieldElementT ElementFromInts(uint64_t coef0, uint64_t coef1) {
  return ExtensionFieldElement(
      TestFieldElement::FromUint(coef0), TestFieldElement::FromUint(coef1));
}

TEST(ExtensionFieldElement, Equality) {
  Prng prng;
  const auto a = ExtensionFieldElementT::RandomElement(&prng);
  const auto b = ExtensionFieldElementT::RandomElement(&prng);
  EXPECT_TRUE(a == a);
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(b == b);
  EXPECT_TRUE(b != a);
}

TEST(ExtensionFieldElement, Addition) {
  Prng prng;
  const auto a = ExtensionFieldElementT::RandomElement(&prng);
  const auto b = ExtensionFieldElementT::RandomElement(&prng);
  EXPECT_EQ(
      a + b, ExtensionFieldElementT(a.GetCoef0() + b.GetCoef0(), a.GetCoef1() + b.GetCoef1()));
}

TEST(ExtensionFieldElement, Subtraction) {
  Prng prng;
  const auto a = ExtensionFieldElementT::RandomElement(&prng);
  const auto b = ExtensionFieldElementT::RandomElement(&prng);
  const auto c = ExtensionFieldElementT(a.GetCoef0() - b.GetCoef0(), a.GetCoef1() - b.GetCoef1());
  EXPECT_EQ(a - b, c);
  EXPECT_EQ(b - a, -c);
}

TEST(ExtensionFieldElement, Inverse) {
  Prng prng;
  const auto a = ElementFromInts(2352037337, 2044312682);
  const auto b = ExtensionFieldElementT::RandomElement(&prng);
  const auto a_inv = ElementFromInts(2626718436, 2471080172);
  EXPECT_EQ(a.Inverse(), a_inv);
  EXPECT_EQ(a, a_inv.Inverse());
  EXPECT_EQ(b, b.Inverse().Inverse());
}

TEST(ExtensionFieldElement, Multiplication) {
  Prng prng;
  const auto a = ElementFromInts(1080896270, 3075192004);
  const auto b = ElementFromInts(2530508219, 2534583573);
  const auto c = ElementFromInts(174080541, 2067416996);
  EXPECT_EQ(a * b, c);
  EXPECT_EQ(a, a * b * b.Inverse());
}

TEST(ExtensionFieldElement, MultiplicationZeroCoefs) {
  Prng prng;
  const auto a = ElementFromInts(1080896270, 0);
  const auto b = ElementFromInts(2530508219, 0);
  const auto c = ElementFromInts(113226290, 0);
  EXPECT_EQ(a * b, c);
  EXPECT_EQ(a, a * b * b.Inverse());
  const auto d = ElementFromInts(0, 1995620035);
  const auto e = ElementFromInts(0, 1102816823);
  const auto f = ElementFromInts(2329323244, 0);
  EXPECT_EQ(d * e, f);
  EXPECT_EQ(d, d * e * e.Inverse());
  const auto g = ElementFromInts(1642651854, 0);
  const auto h = ElementFromInts(0, 2603621674);
  const auto i = ElementFromInts(0, 53019620);
  EXPECT_EQ(g * h, i);
  EXPECT_EQ(g * h, h * g);
  const auto j = ElementFromInts(796837448, 968063499);
  const auto k = ElementFromInts(780243623, 0);
  const auto l = ElementFromInts(395268440, 1799352989);
  EXPECT_EQ(j * k, l);
  EXPECT_EQ(j * k, k * j);
}

TEST(ExtensionFieldElement, Division) {
  Prng prng;
  const auto a = ElementFromInts(1455313761, 621314519);
  const auto b = ElementFromInts(181960078, 1862804457);
  const auto c = ElementFromInts(2877980588, 2293592908);
  const auto a_div_b = a / b;
  EXPECT_EQ(a_div_b, c);
  EXPECT_EQ(a, a_div_b * b);
}

}  // namespace
}  // namespace starkware
