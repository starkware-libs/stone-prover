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

#include "starkware/fft_utils/fft_bases.h"

#include <utility>
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {

using testing::ElementsAre;
using testing::HasSubstr;

/*
  Tests that the domains in Reversered order bases were derived correctly from the domain
  start_offset*<g> where g is an element of order 8.
*/
void TestReverseredOrderMultiplicativeBases(
    const TestFieldElement& g, const TestFieldElement& start_offset,
    const FftBasesDefaultImpl<TestFieldElement>& bases) {
  ASSERT_EQ(3U, bases.NumLayers());
  ASSERT_EQ(8U, bases[0].Size());

  ASSERT_EQ(3U, bases[0].Basis().size());

  // Check that g is of order 8.
  EXPECT_EQ(TestFieldElement::One(), Pow(g, 8));
  EXPECT_NE(TestFieldElement::One(), Pow(g, 4));

  EXPECT_THAT(bases[0].Basis(), ElementsAre(Pow(g, 4), Pow(g, 2), g));
  EXPECT_EQ(bases[0].StartOffset(), start_offset);

  EXPECT_THAT(bases[1].Basis(), ElementsAre(Pow(g, 4), Pow(g, 2)));
  EXPECT_EQ(Pow(start_offset, 2), bases[1].StartOffset());
  EXPECT_EQ(Pow(start_offset, 2), bases.ApplyBasisTransformTmpl(bases[0].StartOffset(), 0));

  EXPECT_THAT(bases[2].Basis(), ElementsAre(Pow(g, 4)));
  EXPECT_EQ(Pow(start_offset, 4), bases[2].StartOffset());
  EXPECT_EQ(Pow(start_offset, 4), bases.ApplyBasisTransformTmpl(bases[1].StartOffset(), 1));
  EXPECT_EQ(
      Pow(start_offset, 4), bases.FromLayer(1).ApplyBasisTransformTmpl(bases[1].StartOffset(), 0));

  EXPECT_EQ(bases[3].Basis().size(), 0U);
  EXPECT_EQ(Pow(start_offset, 8), bases[3].StartOffset());
  EXPECT_EQ(Pow(start_offset, 8), bases.ApplyBasisTransformTmpl(bases[2].StartOffset(), 2));
  EXPECT_EQ(
      Pow(start_offset, 8), bases.FromLayer(2).ApplyBasisTransformTmpl(bases[2].StartOffset(), 0));
}

/*
  Tests that the domains in Natural Order bases were derived correctly from the domain
  start_offset*<g> where g is an element of order 8.
*/
template <typename BasesT>
void TestNaturalOrderMultiplicativeBases(
    const TestFieldElement& g, const TestFieldElement& start_offset, const BasesT& bases) {
  ASSERT_EQ(3U, bases.NumLayers());
  ASSERT_EQ(8U, bases[0].Size());

  ASSERT_EQ(3U, bases[0].Basis().size());

  // Check that g is of order 8.
  EXPECT_EQ(TestFieldElement::One(), Pow(g, 8));
  EXPECT_NE(TestFieldElement::One(), Pow(g, 4));

  EXPECT_THAT(bases[0].Basis(), ElementsAre(g, Pow(g, 2), Pow(g, 4)));
  EXPECT_EQ(bases[0].StartOffset(), start_offset);

  EXPECT_THAT(bases[1].Basis(), ElementsAre(Pow(g, 2), Pow(g, 4)));
  EXPECT_EQ(Pow(start_offset, 2), bases[1].StartOffset());
  EXPECT_EQ(Pow(start_offset, 2), bases.ApplyBasisTransformTmpl(bases[0].StartOffset(), 0));

  EXPECT_THAT(bases[2].Basis(), ElementsAre(Pow(g, 4)));
  EXPECT_EQ(Pow(start_offset, 4), bases[2].StartOffset());
  EXPECT_EQ(Pow(start_offset, 4), bases.ApplyBasisTransformTmpl(bases[1].StartOffset(), 1));
  EXPECT_EQ(
      Pow(start_offset, 4), bases.FromLayer(1).ApplyBasisTransformTmpl(bases[1].StartOffset(), 0));

  EXPECT_EQ(bases[3].Basis().size(), 0U);
  EXPECT_EQ(Pow(start_offset, 8), bases[3].StartOffset());
  EXPECT_EQ(Pow(start_offset, 8), bases.ApplyBasisTransformTmpl(bases[2].StartOffset(), 2));
  EXPECT_EQ(
      Pow(start_offset, 8), bases.FromLayer(2).ApplyBasisTransformTmpl(bases[2].StartOffset(), 0));
}

TEST(FftBases, Multiplicative) {
  Prng prng;
  const auto start_offset = TestFieldElement::RandomElement(&prng);
  const auto bases = MakeFftBases(3, start_offset);
  const auto& domain = bases[0];
  TestReverseredOrderMultiplicativeBases(domain.Basis().back(), start_offset, bases);

  const auto inv_generator = domain.Basis().back().Inverse();
  TestReverseredOrderMultiplicativeBases(
      inv_generator, start_offset, MakeFftBases(inv_generator, 3, start_offset));

  TestNaturalOrderMultiplicativeBases(
      inv_generator, start_offset,
      MultiplicativeFftBases<TestFieldElement, MultiplicativeGroupOrdering::kNaturalOrder>(
          inv_generator, bases.NumLayers(), start_offset));
}

template <typename FieldElementT>
void TestGetShiftedBases(size_t log_n) {
  Prng prng;
  const auto offset = FieldElementT::RandomElement(&prng);
  auto fft_bases = MakeFftBases(log_n, FieldElementT::RandomElement(&prng));
  auto fft_bases1 = MakeFftBases(log_n, offset);
  auto fft_bases2 = fft_bases.GetShiftedBases(offset);

  for (size_t i = 0; i <= fft_bases1.NumLayers(); i++) {
    EXPECT_EQ(fft_bases1[i].StartOffset(), fft_bases2[i].StartOffset());
    EXPECT_EQ(fft_bases1[i].Basis(), fft_bases2[i].Basis());
  }
}

TEST(FftBases, GetShiftedBases) { TestGetShiftedBases<TestFieldElement>(4); }

TEST(FftBases, FromLayer) {
  Prng prng;
  const auto start_offset = TestFieldElement::RandomElement(&prng);
  const auto bases = MakeFftBases(3, start_offset);

  EXPECT_ASSERT(bases.FromLayer(4), HasSubstr("index out of range"));
  const auto bases1 = bases.FromLayer(1);
  ASSERT_EQ(bases1.NumLayers(), 2);
  for (size_t i = 0; i <= 2; ++i) {
    EXPECT_EQ(bases[i + 1].StartOffset(), bases1[i].StartOffset());
    EXPECT_EQ(bases[i + 1].Basis(), bases1[i].Basis());
  }
  const auto bases3 = bases.FromLayer(3);
  EXPECT_EQ(bases3.NumLayers(), 0);
}

using BasesTypes = ::testing::Types<
    MultiplicativeFftBases<TestFieldElement, MultiplicativeGroupOrdering::kBitReversedOrder>,
    MultiplicativeFftBases<TestFieldElement, MultiplicativeGroupOrdering::kNaturalOrder>>;

template <typename>
class FftBasesTyped : public ::testing::Test {};

TYPED_TEST_CASE(FftBasesTyped, BasesTypes);

template <typename BasesT, typename FieldElementT = typename BasesT::FieldElementT>
void TestSplit(typename BasesT::DomainT domain, size_t n) {
  bool reversed = std::is_same<
      BasesT,
      MultiplicativeFftBases<FieldElementT, MultiplicativeGroupOrdering::kNaturalOrder>>::value;
  using DomainT = typename BasesT::DomainT;
  using GroupT = typename BasesT::GroupT;
  const auto& [coset_bases, offsets_domain] = BasesT::SplitDomain(domain, n);
  EXPECT_EQ(offsets_domain.Size(), 1U << n);
  ASSERT_EQ(coset_bases.NumLayers(), domain.BasisSize() - n);
  EXPECT_EQ(coset_bases[0].BasisSize(), domain.BasisSize() - n);

  // Test iterations.
  std::vector<FieldElementT> split_elements;
  DomainT coset_domain = coset_bases[0];
  if (coset_domain.BasisSize() > 0) {
    ASSERT_TRUE(
        coset_domain.Basis().front() == -FieldElementT::One() ||
        coset_domain.Basis().back() == -FieldElementT::One());
  }

  const DomainT* domain_a = &offsets_domain;
  const DomainT* domain_b = &coset_domain;
  if (reversed) {
    std::swap(domain_a, domain_b);
  }
  for (const FieldElementT& element_a : *domain_a) {
    for (const FieldElementT& element_b : *domain_b) {
      split_elements.push_back(GroupT::GroupOperation(element_a, element_b));
    }
  }
  std::vector<FieldElementT> domain_elements{domain.begin(), domain.end()};
  EXPECT_EQ(split_elements, domain_elements);
}

TYPED_TEST(FftBasesTyped, SplitDomain) {
  using BasesT = TypeParam;
  using FieldElementT = typename BasesT::FieldElementT;

  Prng prng;
  const auto start_offset = FieldElementT::RandomElement(&prng);
  const BasesT bases = BasesT(3, start_offset);
  EXPECT_NO_THROW(bases.SplitToCosets(0));
  EXPECT_NO_THROW(bases.SplitToCosets(3));
  EXPECT_ASSERT(bases.SplitToCosets(4), HasSubstr("Too many cosets requested"));
  EXPECT_NO_THROW(bases.FromLayer(3).SplitToCosets(0));
  TestSplit<BasesT>(bases[0], 0U);
  TestSplit<BasesT>(bases[0], 1U);
  TestSplit<BasesT>(bases[0], 2U);
  TestSplit<BasesT>(bases[0], 3U);
}

TYPED_TEST(FftBasesTyped, Invoke) {
  using BasesTOrig = TypeParam;
  using FieldElementT = typename BasesTOrig::FieldElementT;

  Prng prng;
  const auto start_offset = FieldElementT::RandomElement(&prng);
  const BasesTOrig bases_orig = BasesTOrig(3, start_offset);
  InvokeBasesTemplateVersion(
      [&](const auto& bases) {
        using BasesT = std::remove_const_t<std::remove_reference_t<decltype(bases)>>;
        bool type_is_correct = std::is_same_v<BasesT, BasesTOrig>;
        EXPECT_TRUE(type_is_correct);
        EXPECT_EQ(bases.NumLayers(), bases_orig.NumLayers());
      },
      bases_orig);
}

}  // namespace starkware
