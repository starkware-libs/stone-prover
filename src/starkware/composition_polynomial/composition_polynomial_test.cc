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

#include "starkware/composition_polynomial/composition_polynomial.h"

#include <optional>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/air.h"
#include "starkware/air/test_utils.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::HasSubstr;

constexpr uint64_t kDefaultGroupOrder = 4;
using FieldElementT = TestFieldElement;
using DummyAirT = DummyAir<FieldElementT>;

TEST(CompositionPolynomial, ZeroConstraints) {
  Prng prng;

  DummyAirT air(kDefaultGroupOrder);
  air.composition_polynomial_degree_bound = 1000;
  air.n_constraints = 0;
  const std::unique_ptr<CompositionPolynomial> poly = air.CreateCompositionPolynomial(
      FieldElement(FieldElementT::One()), FieldElementVector::Make<FieldElementT>({}));
  const FieldElementT evaluation_point = FieldElementT::RandomElement(&prng);

  EXPECT_EQ(
      FieldElement(FieldElementT::Zero()),
      poly->EvalAtPoint(
          FieldElement(evaluation_point), FieldElementVector::Make<FieldElementT>({})));
}

TEST(CompositionPolynomial, shifts) {
  Prng prng;

  DummyAirT air(kDefaultGroupOrder);
  air.composition_polynomial_degree_bound = 1000;
  air.n_constraints = 0;
  const std::unique_ptr<CompositionPolynomial> poly = air.CreateCompositionPolynomial(
      FieldElement(FieldElementT::One()), FieldElementVector::Make<FieldElementT>({}));
}

TEST(CompositionPolynomial, WrongNumCoefficients) {
  Prng prng;

  DummyAirT air(kDefaultGroupOrder);
  air.n_constraints = 1;

  const FieldElementVector coefficients =
      FieldElementVector::Make(prng.RandomFieldElementVector<FieldElementT>(air.n_constraints + 3));

  EXPECT_ASSERT(
      air.CreateCompositionPolynomial(coefficients), HasSubstr("Wrong number of coefficients."));
}

PeriodicColumn<FieldElementT> RandomPeriodicColumn(
    Prng* prng, const std::optional<size_t>& log_coset_size_opt = std::nullopt) {
  const size_t log_coset_size = log_coset_size_opt ? *log_coset_size_opt : prng->UniformInt(0, 4);
  const auto log_n_values = prng->template UniformInt<size_t>(0, log_coset_size);
  const auto group_generator = GetSubGroupGenerator<FieldElementT>(Pow2(log_coset_size));
  const auto offset = FieldElementT::RandomElement(prng);

  return PeriodicColumn<FieldElementT>(
      prng->RandomFieldElementVector<FieldElementT>(Pow2(log_n_values)), group_generator, offset,
      Pow2(log_coset_size), 1);
}

void TestEvalCompositionOnCoset(const size_t log_coset_size, const uint64_t task_size) {
  Prng prng;

  const size_t n_columns = prng.UniformInt(1, 20);
  const size_t trace_length = Pow2(log_coset_size);

  DummyAir<FieldElementT> air(trace_length);
  air.n_constraints = 1;
  air.periodic_columns.push_back(RandomPeriodicColumn(&prng, log_coset_size));
  air.n_columns = n_columns;
  air.mask = {{{0, 0}, {1, 0}}};

  air.composition_polynomial_degree_bound = 2 * trace_length;

  air.point_exponents = {trace_length};  // Used to compute everywhere.
  air.constraints = {
      [](gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
         gsl::span<const FieldElementT> random_coefficients, const FieldElementT& /*point*/,
         gsl::span<const FieldElementT> /*gen_power*/,
         gsl::span<const FieldElementT> precomp_evals) {
        const FieldElementT constraint = (neighbors[0] * periodic_columns[0]) - neighbors[1];

        // Nowhere.
        const FieldElementT& numerator = FieldElementT::One();

        // Everywhere.
        const FieldElementT denominator = precomp_evals[0];

        return FractionFieldElement<FieldElementT>(
            constraint * random_coefficients[0] * numerator, denominator);
      }};

  const FieldElementT coset_group_generator = GetSubGroupGenerator<FieldElementT>(trace_length);

  const FieldElementVector coefficients = FieldElementVector::Make(
      prng.RandomFieldElementVector<FieldElementT>(air.NumRandomCoefficients()));

  const std::unique_ptr<CompositionPolynomial> poly =
      air.CreateCompositionPolynomial(FieldElement(coset_group_generator), coefficients);
  const FieldElementT coset_offset = FieldElementT::RandomElement(&prng);

  // Construct random trace LDE.
  std::vector<FieldElementVector> trace_lde;
  for (size_t i = 0; i < n_columns; ++i) {
    trace_lde.push_back(
        FieldElementVector::Make(prng.RandomFieldElementVector<FieldElementT>(trace_length)));
  }

  // Evaluate on coset.
  FieldElementVector evaluation =
      FieldElementVector::MakeUninitialized(Field::Create<FieldElementT>(), trace_length);
  poly->EvalOnCosetBitReversedOutput(
      FieldElement(coset_offset),
      std::vector<ConstFieldElementSpan>(trace_lde.begin(), trace_lde.end()), evaluation,
      task_size);

  for (size_t i = 0; i < trace_length; ++i) {
    // Collect neighbors.
    std::vector<FieldElementT> neighbors;
    neighbors.reserve(air.mask.size());
    for (const auto& mask_item : air.mask) {
      neighbors.push_back(trace_lde[mask_item.second][Modulo(i + mask_item.first, trace_length)]
                              .As<FieldElementT>());
    }
    // Evaluate and test.
    const FieldElementT curr_point = coset_offset * Pow(coset_group_generator, i);
    ASSERT_EQ(
        poly->EvalAtPoint(
            FieldElement(curr_point),
            ConstFieldElementSpan(gsl::span<const FieldElementT>(neighbors))),
        evaluation[BitReverse(i, log_coset_size)]);
  }
}

TEST(CompositionPolynomial, EvalCompositionOnCoset) {
  Prng prng;
  TestEvalCompositionOnCoset(prng.UniformInt(5, 8), Pow2(4));

  // task_size is not a power of 2.
  TestEvalCompositionOnCoset(prng.UniformInt(5, 8), 20);
  // task_size > coset_size.
  TestEvalCompositionOnCoset(4, Pow2(5));
}

}  // namespace
}  // namespace starkware
