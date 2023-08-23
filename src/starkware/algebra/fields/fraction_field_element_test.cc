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

#include <vector>

#include "starkware/algebra/fields/fraction_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace starkware {
namespace {

using FractionFieldElementT = FractionFieldElement<TestFieldElement>;

FractionFieldElementT ElementFromInts(uint32_t numerator, uint32_t denominator) {
  FractionFieldElementT num(TestFieldElement::FromUint(numerator));
  FractionFieldElementT denom(TestFieldElement::FromUint(denominator));
  return num * (denom.Inverse());
}

TEST(FractionFieldElement, Equality) {
  FractionFieldElementT a = ElementFromInts(5, 10);
  FractionFieldElementT b = ElementFromInts(30, 60);
  FractionFieldElementT c = ElementFromInts(1, 3);
  FractionFieldElementT d =
      ElementFromInts(TestFieldElement::kModulus - 1, TestFieldElement::kModulus - 2);
  EXPECT_TRUE(a == b);
  EXPECT_TRUE(a == d);
  EXPECT_FALSE(a != d);
  EXPECT_FALSE(a != b);
  EXPECT_FALSE(a == c);
  EXPECT_TRUE(a != c);
  EXPECT_FALSE(b == c);
  EXPECT_TRUE(b != c);
}

TEST(FractionFieldElement, Addition) {
  EXPECT_EQ(ElementFromInts(1, 3) + ElementFromInts(5, 4), ElementFromInts(19, 12));
}

TEST(FractionFieldElement, UnaryMinus) {
  EXPECT_EQ(FractionFieldElementT::Zero(), -FractionFieldElementT::Zero());
  Prng prng;
  for (size_t i = 0; i < 100; ++i) {
    const auto max_range = TestFieldElement::kModulus - 1;
    FractionFieldElementT x = ElementFromInts(
        prng.UniformInt<uint32_t>(1, max_range), prng.UniformInt<uint32_t>(1, max_range));
    ASSERT_NE(x, -x);
    ASSERT_EQ(FractionFieldElementT::Zero(), -x + x);
  }
}

TEST(FractionFieldElement, Subtraction) {
  FractionFieldElementT a = ElementFromInts(5, 2);
  FractionFieldElementT b = ElementFromInts(1, 3);
  FractionFieldElementT c = ElementFromInts(13, 6);
  EXPECT_EQ(a - b, c);
  EXPECT_EQ(b - a, -c);
}

TEST(FractionFieldElement, Multiplication) {
  FractionFieldElementT a = ElementFromInts(1, 3);
  FractionFieldElementT b = ElementFromInts(6, 4);
  FractionFieldElementT c1 = ElementFromInts(6, 12);
  FractionFieldElementT c2 = ElementFromInts(1, 2);
  FractionFieldElementT c3 = ElementFromInts(2, 4);
  FractionFieldElementT a_mul_b = a * b;
  EXPECT_EQ(a_mul_b, c1);
  EXPECT_EQ(a_mul_b, c2);
  EXPECT_EQ(a_mul_b, c3);
  EXPECT_EQ(a, a_mul_b * b.Inverse());
}

TEST(FractionFieldElement, Inverse) {
  FractionFieldElementT a = ElementFromInts(6, 4);
  FractionFieldElementT b = ElementFromInts(10, 1);
  FractionFieldElementT c = ElementFromInts(1, 1);
  FractionFieldElementT d = ElementFromInts(0, 1);
  FractionFieldElementT e = ElementFromInts(12, 18);
  FractionFieldElementT a_inv = a.Inverse();
  FractionFieldElementT b_inv = b.Inverse();
  FractionFieldElementT c_inv = c.Inverse();
  FractionFieldElementT e_inv = e.Inverse();
  Prng prng;
  FractionFieldElementT random = FractionFieldElementT::RandomElement(&prng);
  EXPECT_EQ(a_inv, e);
  EXPECT_EQ(a, e_inv);
  EXPECT_EQ(FractionFieldElementT::One(), a_inv * a);
  EXPECT_EQ(FractionFieldElementT::One(), b * b_inv);
  EXPECT_EQ(c, c_inv);
  EXPECT_EQ(random, random.Inverse().Inverse());
  EXPECT_ASSERT(d.Inverse(), testing::HasSubstr("Zero does not have an inverse"));
}

TEST(FractionFieldElement, Division) {
  FractionFieldElementT a = ElementFromInts(5, 10);
  FractionFieldElementT b = ElementFromInts(6, 4);
  FractionFieldElementT c = ElementFromInts(1, 3);
  FractionFieldElementT a_div_b = a / b;
  EXPECT_EQ(a_div_b, c);
  EXPECT_EQ(a, a_div_b * b);
}

TEST(FractionFieldElement, ToBaseFieldElement) {
  Prng prng;
  TestFieldElement a = TestFieldElement::RandomElement(&prng);
  TestFieldElement b = TestFieldElement::RandomElement(&prng);
  EXPECT_EQ(FractionFieldElementT(a).ToBaseFieldElement(), a);
  EXPECT_EQ(
      FractionFieldElementT(TestFieldElement::Zero()).ToBaseFieldElement(),
      TestFieldElement::Zero());
  EXPECT_EQ(
      FractionFieldElementT(TestFieldElement::One()).ToBaseFieldElement(), TestFieldElement::One());
  EXPECT_EQ(
      (FractionFieldElementT(a) / FractionFieldElementT(b)).ToBaseFieldElement(), a * b.Inverse());
}

/*
  Returns a random fraction field element with a random non zero denominator. Note that
  FractionFieldElementT::RandomElement(prng) always returns an element with denominator 1.
*/
FractionFieldElementT RandomFractionalElement(Prng* prng) {
  return FractionFieldElementT::RandomElement(prng) *
         FractionFieldElementT(RandomNonZeroElement<TestFieldElement>(prng)).Inverse();
}

TEST(FractionFieldElement, BatchToBaseFieldElement) {
  Prng prng;
  // Initialize input, ouput and expected output vectors.
  const size_t n_cols = prng.UniformInt(0, 10);
  std::vector<std::vector<FractionFieldElementT>> input(n_cols);
  std::vector<std::vector<TestFieldElement>> expected_output(n_cols);
  std::vector<std::vector<TestFieldElement>> output(n_cols);
  for (size_t i = 0; i < n_cols; i++) {
    const size_t n_rows = prng.UniformInt(0, 10);
    for (size_t j = 0; j < n_rows; j++) {
      const FractionFieldElementT input_val = RandomFractionalElement(&prng);
      input[i].push_back(input_val);
      expected_output[i].push_back(input_val.ToBaseFieldElement());
      output[i].push_back(TestFieldElement::Zero());
    }
  }
  FractionFieldElementT::BatchToBaseFieldElement(
      std::vector<gsl::span<const FractionFieldElementT>>(input.begin(), input.end()),
      std::vector<gsl::span<TestFieldElement>>(output.begin(), output.end()));
  EXPECT_EQ(expected_output, output);
}

/*
  Verifies nothing bad happens when the batch is empty.
*/
TEST(FractionFieldElement, BatchToBaseFieldElementEmptyMatrix) {
  FractionFieldElementT::BatchToBaseFieldElement(
      std::vector<gsl::span<const FractionFieldElementT>>{},
      std::vector<gsl::span<TestFieldElement>>{});
}

/*
  Verifies nothing bad happens when the input contains only empty spans.
*/
TEST(FractionFieldElement, BatchToBaseFieldElementOfEmptySpans) {
  Prng prng;
  const size_t n_cols = prng.UniformInt(0, 10);
  std::vector<std::vector<FractionFieldElementT>> input(n_cols);
  std::vector<std::vector<TestFieldElement>> expected_output(n_cols);
  std::vector<std::vector<TestFieldElement>> output(n_cols);

  FractionFieldElementT::BatchToBaseFieldElement(
      std::vector<gsl::span<const FractionFieldElementT>>(input.begin(), input.end()),
      std::vector<gsl::span<TestFieldElement>>(output.begin(), output.end()));
  EXPECT_EQ(expected_output, output);
}

}  // namespace
}  // namespace starkware
