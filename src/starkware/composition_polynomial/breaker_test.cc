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

#include "starkware/composition_polynomial/breaker.h"

#include <optional>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/lde/lde_manager_impl.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

/*
  1. Take a random evaluation f(x).
  2. Break to h_i(x) evaluations using PolynomialBreakTmpler::Break().
  3. Pick random points x_j. Compute f(x_j), and h_i(x_j^(2^log_breaks)) using LdeManagers.
  4. Use PolynomialBreakTmpler::EvalFromSamples() to assemble f(x_j), and compare to result from
  previous step.
*/
template <MultiplicativeGroupOrdering Order>
void TestPolynomialBreak(
    const size_t log_domain, const size_t log_breaks, const size_t n_check_points) {
  using BasesT = MultiplicativeFftBases<TestFieldElement, Order>;
  Prng prng;
  BasesT bases(log_domain, TestFieldElement::RandomElement(&prng));
  auto broken_bases = bases.FromLayer(log_breaks);
  auto poly_break = MakePolynomialBreak(bases, log_breaks);

  // Step 1. Take a random evaluation f(x).
  auto evaluation = prng.RandomFieldElementVector<TestFieldElement>(Pow2(log_domain));

  // Step 2. Break to h_i(x) evaluations using PolynomialBreakTmpler::Break().
  std::vector<TestFieldElement> break_storage(evaluation.size(), TestFieldElement::Uninitialized());
  auto broken_evals = poly_break->Break(
      FieldElementVector::CopyFrom(evaluation), FieldElementSpan(gsl::make_span(break_storage)));

  // Step 3.
  // Pick random points x_j.
  auto points = prng.RandomFieldElementVector<TestFieldElement>(n_check_points);

  // Compute f(x_j).
  LdeManagerTmpl<MultiplicativeLde<Order, TestFieldElement>> lde_manager(bases);
  lde_manager.AddEvaluation(evaluation);

  std::vector<TestFieldElement> expected_results(n_check_points, TestFieldElement::Uninitialized());
  lde_manager.EvalAtPoints(0, points, expected_results);

  // Prepare to compute h_i(x_j^(2^log_breaks)).
  LdeManagerTmpl<MultiplicativeLde<Order, TestFieldElement>> broken_lde_manager(broken_bases);

  for (const auto& broken_eval : broken_evals) {
    broken_lde_manager.AddEvaluation(broken_eval, nullptr);
  }

  // Compute x_j^(2^log_breaks).
  std::vector<TestFieldElement> points_transformed;
  points_transformed.reserve(n_check_points);
  for (const TestFieldElement& point : points) {
    points_transformed.emplace_back(Pow(point, Pow2(log_breaks)));
  }

  // Compute h_i(x_j^(2^log_breaks)).
  std::vector<std::vector<TestFieldElement>> sub_results;
  sub_results.reserve(Pow2(log_breaks));
  for (size_t i = 0; i < Pow2(log_breaks); ++i) {
    sub_results.emplace_back(n_check_points, TestFieldElement::Uninitialized());
    broken_lde_manager.EvalAtPoints(i, points_transformed, sub_results.back());
  }

  // Step 4.
  std::vector<TestFieldElement> samples(Pow2(log_breaks), TestFieldElement::Uninitialized());
  for (size_t j = 0; j < n_check_points; ++j) {
    for (size_t i = 0; i < Pow2(log_breaks); ++i) {
      samples[i] = sub_results[i][j];
    }
    // Use PolynomialBreakTmpler::EvalFromSamples() to assemble f(x_j).
    TestFieldElement actual_result =
        poly_break
            ->EvalFromSamples(
                ConstFieldElementSpan(gsl::span<const TestFieldElement>(samples)),
                FieldElement(points[j]))
            .template As<TestFieldElement>();

    // Compare to result from previous step.
    EXPECT_EQ(actual_result, expected_results[j]);
  }
}

TEST(PolynomialBreak, Basic) {
  TestPolynomialBreak<MultiplicativeGroupOrdering::kNaturalOrder>(5, 0, 10);
  TestPolynomialBreak<MultiplicativeGroupOrdering::kNaturalOrder>(5, 3, 10);
  TestPolynomialBreak<MultiplicativeGroupOrdering::kNaturalOrder>(5, 5, 10);

  TestPolynomialBreak<MultiplicativeGroupOrdering::kBitReversedOrder>(5, 0, 10);
  TestPolynomialBreak<MultiplicativeGroupOrdering::kBitReversedOrder>(5, 3, 10);
  TestPolynomialBreak<MultiplicativeGroupOrdering::kBitReversedOrder>(5, 5, 10);
}

}  // namespace
}  // namespace starkware
