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


#include "starkware/fft_utils/fft_domain.h"

#include <vector>

#include "third_party/cppitertools/enumerate.hpp"
#include "third_party/cppitertools/zip.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::ElementsAre;
using testing::ElementsAreArray;

using TestedField = TestFieldElement;

/*
  An auxiliary function to make tests less cumbersome to read.
*/
std::vector<TestedField> IntVecToTestedFieldVec(const std::vector<int>& v) {
  std::vector<TestedField> sfe_vec;
  sfe_vec.reserve(v.size());
  for (const int i : v) {
    sfe_vec.push_back(TestedField::FromUint(i));
  }
  return sfe_vec;
}

TEST(FftDomain, Multiplicative) {
  auto fft_domain =
      MakeMultiplicativeFftDomain(IntVecToTestedFieldVec({2, 3, 5}), TestedField::FromUint(1));

  EXPECT_THAT(
      std::vector<TestedField>(fft_domain.begin(), fft_domain.end()),
      ElementsAreArray(IntVecToTestedFieldVec({1, 2, 3, 6, 5, 10, 15, 30})));
  EXPECT_EQ(fft_domain.Size(), 8U);
}

/*
  Tests operator[].
*/
TEST(FftDomain, MultuplicativeOperatorAt) {
  auto fft_domain_multiplicative =
      MakeMultiplicativeFftDomain(IntVecToTestedFieldVec({2, 3, 5}), TestedField::FromUint(1));
  // Compare operator[] with the iterator.
  for (const auto&& [i, x] : iter::enumerate(fft_domain_multiplicative)) {
    ASSERT_EQ(fft_domain_multiplicative[i], x);
    ASSERT_EQ(
        FieldElement(fft_domain_multiplicative[i]), fft_domain_multiplicative.GetFieldElementAt(i));
  }
}

TEST(FftDomain, RemoveFirstBasisElements) {
  auto fft_domain =
      MakeMultiplicativeFftDomain(IntVecToTestedFieldVec({2, 3, 5}), TestedField::FromUint(1));

  auto domain_removed0 = fft_domain.RemoveFirstBasisElements(0);
  EXPECT_THAT(
      std::vector<TestedField>(domain_removed0.begin(), domain_removed0.end()),
      ElementsAreArray(IntVecToTestedFieldVec({1, 2, 3, 6, 5, 10, 15, 30})));
  auto domain_removed1 = fft_domain.RemoveFirstBasisElements(1);
  EXPECT_THAT(
      std::vector<TestedField>(domain_removed1.begin(), domain_removed1.end()),
      ElementsAreArray(IntVecToTestedFieldVec({1, 3, 5, 15})));
  auto domain_removed2 = fft_domain.RemoveFirstBasisElements(2);
  EXPECT_THAT(
      std::vector<TestedField>(domain_removed2.begin(), domain_removed2.end()),
      ElementsAreArray(IntVecToTestedFieldVec({1, 5})));
  EXPECT_EQ(domain_removed2.Size(), 2U);
}

TEST(FftDomain, RemoveLastBasisElements) {
  auto fft_domain =
      MakeMultiplicativeFftDomain(IntVecToTestedFieldVec({2, 3, 5}), TestedField::FromUint(1));

  auto domain_removed0 = fft_domain.RemoveLastBasisElements(0);
  EXPECT_THAT(
      std::vector<TestedField>(domain_removed0.begin(), domain_removed0.end()),
      ElementsAreArray(IntVecToTestedFieldVec({1, 2, 3, 6, 5, 10, 15, 30})));
  auto domain_removed1 = fft_domain.RemoveLastBasisElements(1);
  EXPECT_THAT(
      std::vector<TestedField>(domain_removed1.begin(), domain_removed1.end()),
      ElementsAreArray(IntVecToTestedFieldVec({1, 2, 3, 6})));
  auto domain_removed2 = fft_domain.RemoveLastBasisElements(2);
  EXPECT_THAT(
      std::vector<TestedField>(domain_removed2.begin(), domain_removed2.end()),
      ElementsAreArray(IntVecToTestedFieldVec({1, 2})));
  EXPECT_EQ(domain_removed2.Size(), 2U);
}

TEST(FftDomain, GetShiftedDomain) {
  Prng prng;
  auto fft_domain = MakeMultiplicativeFftDomain(
      IntVecToTestedFieldVec({2, 3, 5}), TestedField::RandomElement(&prng));

  auto shifted_domain = fft_domain.GetShiftedDomain(TestedField::FromUint(2));
  EXPECT_EQ(TestedField::FromUint(2), shifted_domain.StartOffset());
  EXPECT_EQ(shifted_domain.Basis(), fft_domain.Basis());
}

TEST(FftDomain, MultiplicativeDomainBySize) {
  size_t log_n = 3;
  Prng prng;

  const auto offset = TestFieldElement::RandomElement(&prng);
  const auto empty_domain = MakeFftDomain(0, offset);

  EXPECT_TRUE(empty_domain.Basis().empty());
  EXPECT_EQ(empty_domain.StartOffset(), offset);

  const auto domain = MakeFftDomain(log_n, offset);
  auto g = GetSubGroupGenerator<TestFieldElement>(Pow2(log_n));

  EXPECT_THAT(domain.Basis(), ElementsAre(Pow(g, 4), Pow(g, 2), g));
  EXPECT_EQ(domain.StartOffset(), offset);
}

TEST(FftDomain, Inverse) {
  size_t log_n = 3;
  Prng prng;

  const auto offset = TestFieldElement::RandomElement(&prng);
  const auto domain = MakeFftDomain(log_n, offset);
  const auto inv_domain = domain.Inverse();

  for (const auto& [x, x_inv] : iter::zip(domain, inv_domain)) {
    EXPECT_EQ(x.Inverse(), x_inv);
  }
}

}  // namespace
}  // namespace starkware
