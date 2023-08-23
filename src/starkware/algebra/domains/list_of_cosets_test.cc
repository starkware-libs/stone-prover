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

#include "starkware/algebra/domains/list_of_cosets.h"

#include <limits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {
namespace {

using testing::HasSubstr;

using TestedFieldTypes =
    ::testing::Types<TestFieldElement, LongFieldElement, PrimeFieldElement<252, 0>>;

template <typename FieldElementT>
class ListOfCosetsTests : public ::testing::Test {};

TYPED_TEST_CASE(ListOfCosetsTests, TestedFieldTypes);

TYPED_TEST(ListOfCosetsTests, CosetSize1) {
  using FieldElementT = TypeParam;
  const Field field = Field::Create<FieldElementT>();
  EXPECT_ASSERT(ListOfCosets::MakeListOfCosets(1, 1, field), HasSubstr("Coset size must be > 1"));
}

TYPED_TEST(ListOfCosetsTests, TwoCosets) {
  using FieldElementT = TypeParam;
  const Field field = Field::Create<FieldElementT>();

  const size_t coset_size = 2;
  const size_t n_cosets = 2;
  const size_t domain_size = coset_size * n_cosets;

  const FieldElement cosets_offsets_generator =
      FieldElement(GetSubGroupGenerator<FieldElementT>(domain_size));
  const FieldElement field_generator = FieldElement(FieldElementT::Generator());
  const FieldElement group_generator =
      MultiplicativeGroup::MakeGroup(coset_size, field).Generator();

  const ListOfCosets domain = ListOfCosets::MakeListOfCosets(coset_size, n_cosets, field);
  EXPECT_EQ(domain.Size(), domain_size);
  EXPECT_EQ(domain.ElementByIndex(0, 0), field_generator);
  EXPECT_EQ(domain.ElementByIndex(0, 1), field_generator * group_generator);
  EXPECT_EQ(domain.ElementByIndex(1, 0), field_generator * cosets_offsets_generator);
  EXPECT_EQ(
      domain.ElementByIndex(1, 1), field_generator * cosets_offsets_generator * group_generator);
  EXPECT_ASSERT(domain.ElementByIndex(0, 2), HasSubstr("Index out of range."));
}

TYPED_TEST(ListOfCosetsTests, ZeroCosets) {
  using FieldElementT = TypeParam;
  const Field field = Field::Create<FieldElementT>();

  Prng prng;
  const size_t coset_size = Pow2(prng.UniformInt(0, 5));
  const size_t n_cosets = 0;

  EXPECT_ASSERT(
      ListOfCosets::MakeListOfCosets(coset_size, n_cosets, field),
      HasSubstr("Number of cosets must be positive."));
}

template <typename FieldElementT>
void TestRandom(MultiplicativeGroupOrdering order) {
  const Field field = Field::Create<FieldElementT>();

  Prng prng;
  const size_t log_coset_size = prng.UniformInt(1, 5);
  const size_t coset_size = Pow2(log_coset_size);
  const size_t n_cosets = prng.UniformInt(1, 10);
  const size_t domain_size = coset_size * n_cosets;

  const FieldElement cosets_offsets_generator =
      FieldElement(GetSubGroupGenerator<FieldElementT>(Pow2(Log2Ceil(domain_size))));

  const ListOfCosets domain = ListOfCosets::MakeListOfCosets(coset_size, n_cosets, field, order);

  EXPECT_EQ(domain.Size(), domain_size);
  EXPECT_ASSERT(domain.ElementByIndex(n_cosets, 0), HasSubstr("Coset index out of range."));

  FieldElement curr_element = FieldElement(FieldElementT::Generator());
  for (size_t coset = 0; coset < n_cosets; coset++) {
    for (size_t g = 0; g < coset_size; g++) {
      size_t domain_index =
          order == MultiplicativeGroupOrdering::kNaturalOrder ? g : BitReverse(g, log_coset_size);
      ASSERT_EQ(curr_element, domain.ElementByIndex(coset, domain_index))
          << "coset = " << coset << ", g = " << g << ", coset_size = " << coset_size
          << ", n_cosets = " << n_cosets;
      curr_element = curr_element * domain.TraceGenerator();
    }
    curr_element = curr_element * cosets_offsets_generator;
  }
}

TYPED_TEST(ListOfCosetsTests, Random) {
  using FieldElementT = TypeParam;

  TestRandom<FieldElementT>(MultiplicativeGroupOrdering::kNaturalOrder);
  TestRandom<FieldElementT>(MultiplicativeGroupOrdering::kBitReversedOrder);
}

}  // namespace
}  // namespace starkware
