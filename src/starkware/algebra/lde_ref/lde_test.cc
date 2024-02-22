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

#include "starkware/algebra/lde/lde.h"

#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/cppitertools/zip.hpp"

#include "starkware/algebra/domains/multiplicative_group.h"
#include "starkware/algebra/fft/multiplicative_group_ordering.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/lde/lde_manager_impl.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/fft_utils/fft_bases.h"
#include "starkware/math/math.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {
namespace {

using testing::ElementsAreArray;

template <typename FieldElementT>
class PrimeFieldLdeTest : public ::testing::Test {};

using PrimeFieldTypes = ::testing::Types<TestFieldElement>;
TYPED_TEST_CASE(PrimeFieldLdeTest, PrimeFieldTypes);

template <typename LdeT>
class LdeTest : public ::testing::Test {
 public:
  using FieldElementT = typename LdeT::T;
  using BasesT = typename LdeT::BasesT;
  BasesT GetBases(size_t log_n, const FieldElementT& offset) { return BasesT(log_n, offset); }
};

using AllLdeTypes = ::testing::Types<
    MultiplicativeLde<MultiplicativeGroupOrdering::kBitReversedOrder, TestFieldElement>,
    MultiplicativeLde<MultiplicativeGroupOrdering::kNaturalOrder, TestFieldElement>>;

TYPED_TEST_CASE(LdeTest, AllLdeTypes);

TYPED_TEST(LdeTest, GetEvaluationDegree) {
  using LdeT = TypeParam;
  using FieldElementT = typename LdeT::T;
  const Field field = Field::Create<FieldElementT>();
  Prng prng;

  const int log_n = 4;
  const size_t n = Pow2(log_n);
  auto bases = this->GetBases(log_n, FieldElementT::RandomElement(&prng));

  for (int64_t degree = -1; degree < static_cast<int64_t>(n); ++degree) {
    auto coefs = prng.RandomFieldElementVector<FieldElementT>(degree + 1);
    std::vector<FieldElementT> src;
    src.reserve(n);
    for (auto x : bases[0]) {
      src.push_back(HornerEval(x, coefs));
    }

    auto lde_manager = LdeManagerTmpl<LdeT>(bases);
    lde_manager.AddEvaluation(ConstFieldElementSpan(gsl::span<const FieldElementT>(src)), nullptr);

    EXPECT_EQ(lde_manager.GetEvaluationDegree(0), degree);
  }
}

TYPED_TEST(LdeTest, GetDomain) {
  using LdeT = TypeParam;
  using FieldElementT = typename LdeT::T;
  const Field field = Field::Create<FieldElementT>();
  Prng prng;

  const int log_n = 4;
  auto bases = this->GetBases(log_n, FieldElementT::RandomElement(&prng));
  auto offset = FieldElementT::RandomElement(&prng);
  auto lde_manager = LdeManagerTmpl<LdeT>(bases);
  auto lde_domain_ptr = lde_manager.GetDomain(FieldElement(offset));
  auto lde_domain = dynamic_cast<typename LdeT::BasesT::DomainT*>(lde_domain_ptr.get());
  ASSERT_NE(lde_domain, nullptr) << "Wrong domain type";

  EXPECT_EQ(lde_domain->StartOffset(), offset);
  EXPECT_EQ(lde_domain->Basis(), bases[0].Basis());
}

TYPED_TEST(LdeTest, Lde_test) {
  using LdeT = TypeParam;
  using FieldElementT = typename LdeT::T;
  const Field field = Field::Create<FieldElementT>();
  Prng prng;

  const int log_n = 4;
  const size_t n = Pow2(log_n);

  auto bases = this->GetBases(log_n, FieldElementT::RandomElement(&prng));

  auto coefs = prng.RandomFieldElementVector<FieldElementT>(n);
  std::vector<FieldElementT> src;
  src.reserve(n);
  for (auto x : bases[0]) {
    src.push_back(HornerEval(x, coefs));
  }

  auto lde_manager = LdeManagerTmpl<LdeT>(bases);
  lde_manager.AddEvaluation(ConstFieldElementSpan(gsl::span<const FieldElementT>(src)), nullptr);

  std::vector<FieldElementT> res = FieldElementT::UninitializedVector(n);
  FieldElementSpan out_span = FieldElementSpan(gsl::make_span(res));

  auto dst_domain = bases[0].GetShiftedDomain(FieldElementT::RandomElement(&prng));
  lde_manager.EvalOnCoset(FieldElement(dst_domain.StartOffset()), {out_span});

  std::vector<FieldElementT> concurrent_res = FieldElementT::UninitializedVector(n);
  out_span = FieldElementSpan(gsl::make_span(concurrent_res));
  auto task_manager = TaskManager::CreateInstanceForTesting();
  lde_manager.EvalOnCoset(
      FieldElement(dst_domain.StartOffset()), {out_span}, nullptr, &task_manager);

  for (const auto& [x, result, concurrent_result] : iter::zip(dst_domain, res, concurrent_res)) {
    FieldElementT expected = HornerEval(x, coefs);
    ASSERT_EQ(expected, result);
    ASSERT_EQ(expected, concurrent_result);
  }
}

TYPED_TEST(LdeTest, lde_with_offset) {
  using LdeT = TypeParam;
  using FieldElementT = typename LdeT::T;
  const Field field = Field::Create<FieldElementT>();
  Prng prng;

  const int log_n = 4;
  const size_t n = Pow2(log_n);

  auto bases = this->GetBases(log_n, FieldElementT::RandomElement(&prng));
  auto lde_manager = LdeManagerTmpl<LdeT>(bases);

  auto coefs = prng.RandomFieldElementVector<FieldElementT>(n);

  lde_manager.AddFromCoefficients(ConstFieldElementSpan(gsl::span<const FieldElementT>(coefs)));

  std::vector<FieldElementT> res = FieldElementT::UninitializedVector(n);
  FieldElementSpan out_span = FieldElementSpan(gsl::make_span(res));
  auto dst_domain = bases[0].GetShiftedDomain(FieldElementT::RandomElement(&prng));
  lde_manager.EvalOnCoset(FieldElement(dst_domain.StartOffset()), {out_span});

  lde_manager.AddEvaluation(ConstFieldElementSpan(gsl::span<const FieldElementT>(res)), nullptr);

  // FFT + IFFT with the diffrent offset should give the diffrent coefficients.
  EXPECT_NE(lde_manager.GetCoefficients(0), lde_manager.GetCoefficients(1));
}

template <typename FieldElementT>
void IdentityTest(size_t log_domain_size) {
  const Field field = Field::Create<FieldElementT>();
  Prng prng;

  const size_t domain_size = Pow2(log_domain_size);
  const MultiplicativeGroup group = MultiplicativeGroup::MakeGroup(domain_size, field);

  auto vec = prng.RandomFieldElementVector<FieldElementT>(domain_size);
  auto trace = FieldElementVector::CopyFrom(vec);
  Field f = Field::Create<FieldElementT>();
  auto lde_manager = MakeLdeManager(group, f.One());

  lde_manager->AddEvaluation(std::move(trace));
  FieldElementVector res = FieldElementVector::MakeUninitialized(f, vec.size());
  auto out_span = res.AsSpan();
  lde_manager->EvalOnCoset(f.One(), {out_span});
  ASSERT_EQ(res.As<FieldElementT>(), vec);
}

TYPED_TEST(PrimeFieldLdeTest, Identity) {
  using FieldElementT = TypeParam;
  IdentityTest<FieldElementT>(4);
}

TYPED_TEST(PrimeFieldLdeTest, kBitReversedOrder) {
  using FieldElementT = TypeParam;
  const Field field = Field::Create<FieldElementT>();
  Prng prng;

  const int log_domain_size = 4;
  const size_t domain_size = Pow2(log_domain_size);
  const MultiplicativeGroup group = MultiplicativeGroup::MakeGroup(domain_size, field);

  auto vec = prng.RandomFieldElementVector<FieldElementT>(domain_size);
  Field f = Field::Create<FieldElementT>();
  auto source_eval_offset = FieldElement(FieldElementT::RandomElement(&prng));

  auto lde_manager = MakeLdeManager(group, source_eval_offset);

  lde_manager->AddEvaluation(FieldElementVector::CopyFrom(vec));
  BitReverseInPlace(gsl::make_span(vec));

  // lde_manager_rev expects the input in reverse order.
  auto lde_manager_rev = MakeBitReversedOrderLdeManager(group, source_eval_offset);
  lde_manager_rev->AddEvaluation(FieldElementVector::Make(std::move(vec)));

  FieldElementVector expected = FieldElementVector::MakeUninitialized(f, domain_size);
  FieldElementVector other = FieldElementVector::MakeUninitialized(f, domain_size);

  auto eval_offset = FieldElement(FieldElementT::RandomElement(&prng));

  auto out_span = expected.AsSpan();
  lde_manager->EvalOnCoset(eval_offset, {out_span});
  out_span = other.AsSpan();
  lde_manager_rev->EvalOnCoset(eval_offset, {out_span});
  BitReverseInPlace(gsl::make_span(other.As<FieldElementT>()));
  EXPECT_EQ(expected, other);
}

template <MultiplicativeGroupOrdering Order, typename FieldElementT>
void EvalAtPointTest(const size_t log_domain_size) {
  const Field field = Field::Create<FieldElementT>();
  Prng prng;

  const size_t domain_size = Pow2(log_domain_size);
  const MultiplicativeGroup group = MultiplicativeGroup::MakeGroup(domain_size, field);

  auto source_eval_offset = FieldElement(FieldElementT::RandomElement(&prng));
  auto lde_manager = Order == MultiplicativeGroupOrdering::kNaturalOrder
                         ? MakeLdeManager(group, source_eval_offset)
                         : MakeBitReversedOrderLdeManager(group, source_eval_offset);

  auto vec = prng.RandomFieldElementVector<FieldElementT>(domain_size);
  if (Order == MultiplicativeGroupOrdering::kBitReversedOrder) {
    // lde_manager_rev expects the input in reverse order.
    BitReverseInPlace(gsl::make_span(vec));
  }
  lde_manager->AddEvaluation(FieldElementVector::Make(std::move(vec)));

  std::vector<FieldElementT> expected_res = FieldElementT::UninitializedVector(domain_size);

  auto out_span = FieldElementSpan(gsl::make_span(expected_res));
  auto eval_offset = FieldElementT::RandomElement(&prng);
  lde_manager->EvalOnCoset(FieldElement(eval_offset), {out_span});

  if (Order == MultiplicativeGroupOrdering::kBitReversedOrder) {
    // Test below expects natural order.
    BitReverseInPlace(gsl::make_span(expected_res));
  }

  LdeManagerTmpl<MultiplicativeLde<Order, FieldElementT>>& lde_tmpl =
      lde_manager->As<MultiplicativeLde<Order, FieldElementT>>();

  FieldElementT w = group.Generator().As<FieldElementT>();

  std::vector<FieldElementT> points;
  FieldElementT offset = eval_offset;
  points.reserve(domain_size);
  for (size_t i = 0; i < domain_size; i++) {
    points.push_back(offset);
    offset = offset * w;
  }

  std::vector<FieldElementT> results(domain_size, FieldElementT::Zero());
  lde_tmpl.EvalAtPoints(0, points, results);
  EXPECT_THAT(results, ElementsAreArray(expected_res));

  std::vector<FieldElementT> results2(domain_size, FieldElementT::Zero());
  lde_tmpl.EvalAtPoints(
      0, ConstFieldElementSpan(gsl::span<const FieldElementT>(points)),
      FieldElementSpan(gsl::make_span(results2)));
  EXPECT_THAT(results2, ElementsAreArray(expected_res));
}

TYPED_TEST(PrimeFieldLdeTest, EvalAtPoint) {
  using FieldElementT = TypeParam;
  EvalAtPointTest<MultiplicativeGroupOrdering::kNaturalOrder, FieldElementT>(4);
  EvalAtPointTest<MultiplicativeGroupOrdering::kNaturalOrder, FieldElementT>(1);
  EvalAtPointTest<MultiplicativeGroupOrdering::kNaturalOrder, FieldElementT>(0);

  EvalAtPointTest<MultiplicativeGroupOrdering::kBitReversedOrder, FieldElementT>(4);
  EvalAtPointTest<MultiplicativeGroupOrdering::kBitReversedOrder, FieldElementT>(1);
  EvalAtPointTest<MultiplicativeGroupOrdering::kBitReversedOrder, FieldElementT>(0);
}

template <MultiplicativeGroupOrdering Order, typename FieldElementT>
void TestAddFromAndGetCoefficients() {
  Prng prng;
  const size_t n = 8;

  std::vector<FieldElementT> coefs = prng.RandomFieldElementVector<FieldElementT>(n);
  FieldElementT source_eval_offset = FieldElementT::RandomElement(&prng);

  MultiplicativeGroup group = MultiplicativeGroup::MakeGroup(n, Field::Create<FieldElementT>());
  auto lde_manager = Order == MultiplicativeGroupOrdering::kNaturalOrder
                         ? MakeLdeManager(group, FieldElement(source_eval_offset))
                         : MakeBitReversedOrderLdeManager(group, FieldElement(source_eval_offset));
  lde_manager->AddFromCoefficients(FieldElementVector::CopyFrom(coefs));

  // Test that GetCoefficients() returns the same coefficients given to AddFromCoefficients().
  auto coefs2 = lde_manager->GetCoefficients(0).As<FieldElementT>();
  EXPECT_THAT(coefs2, ElementsAreArray(coefs));

  // Test that coefs is consistent with EvalAtPoints.
  const FieldElementT point = FieldElementT::RandomElement(&prng);
  FieldElementT output = FieldElementT::Zero();
  lde_manager->EvalAtPoints(
      0, ConstFieldElementSpan(gsl::make_span(&point, 1)),
      FieldElementSpan(gsl::make_span(&output, 1)));

  const FieldElementT fixed_point = point / source_eval_offset;
  FieldElementT expected_output = Order == MultiplicativeGroupOrdering::kNaturalOrder
                                      ? HornerEvalBitReversed(fixed_point, coefs)
                                      : HornerEval(fixed_point, coefs);
  EXPECT_EQ(expected_output, output);
}

TYPED_TEST(PrimeFieldLdeTest, AddFromAndGetCoefficients) {
  using FieldElementT = TypeParam;
  TestAddFromAndGetCoefficients<MultiplicativeGroupOrdering::kNaturalOrder, FieldElementT>();
}

TYPED_TEST(PrimeFieldLdeTest, AddFromAndGetCoefficients_BitReversal) {
  using FieldElementT = TypeParam;
  TestAddFromAndGetCoefficients<MultiplicativeGroupOrdering::kBitReversedOrder, FieldElementT>();
}

}  // namespace
}  // namespace starkware
