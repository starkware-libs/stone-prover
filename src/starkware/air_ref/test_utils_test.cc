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

#include "starkware/air/test_utils.h"

#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::UnorderedElementsAre;

using FieldElementT = TestFieldElement;

TEST(AirTestUtils, ComputeCompositionDegree) {
  // Initialize test parameters.
  Prng prng;

  // Construct simple AIR implementation, over two columns with a single constraint verifying that
  // the second column is a square of the first column over the data domain.
  const size_t trace_length = Pow2(5);
  DummyAir<FieldElementT> air(trace_length);
  air.n_constraints = 1;
  air.n_columns = 2;
  air.mask = {{{0, 0}, {0, 1}}};

  air.composition_polynomial_degree_bound = 2 * trace_length;
  uint64_t constraint_degrees = 2 * trace_length - 2;

  air.point_exponents = {trace_length,  // Used to compute everywhere.
                         (*air.composition_polynomial_degree_bound - 1) -
                             (constraint_degrees +
                              /* nowhere */ 0 - /* everywhere */ trace_length)};
  air.constraints = {[](gsl::span<const FieldElementT> neighbors,
                        gsl::span<const FieldElementT> /*periodic_columns*/,
                        gsl::span<const FieldElementT> random_coefficients,
                        const FieldElementT& /*point*/,
                        gsl::span<const FieldElementT> /*gen_power*/,
                        gsl::span<const FieldElementT> precomp_evals) {
    const FieldElementT constraint = (neighbors[0] * neighbors[0]) - neighbors[1];

    // Nowhere.
    const FieldElementT& numerator = FieldElementT::One();

    // Everywhere.
    const FieldElementT denominator = precomp_evals[0];

    return FractionFieldElement<FieldElementT>(
        constraint * random_coefficients[0] * numerator, denominator);
  }};

  // Construct trace columns.
  const std::vector<FieldElementT> v_rand1 =
      prng.RandomFieldElementVector<FieldElementT>(trace_length);
  std::vector<FieldElementT> v_rand1_sqr;
  v_rand1_sqr.reserve(v_rand1.size());
  for (const auto& e : v_rand1) {
    v_rand1_sqr.push_back(e * e);
  }
  const std::vector<FieldElementT> v_rand2 =
      prng.RandomFieldElementVector<FieldElementT>(trace_length);

  // Draw random coefficients.
  const FieldElementVector rand_coeffs = FieldElementVector::Make(
      prng.RandomFieldElementVector<FieldElementT>(air.NumRandomCoefficients()));

  // Construct traces.
  Trace good_trace(std::vector<std::vector<FieldElementT>>({v_rand1, v_rand1_sqr}));
  Trace bad_trace(std::vector<std::vector<FieldElementT>>({v_rand1, v_rand2}));

  // Verify degrees.
  EXPECT_LT(
      ComputeCompositionDegree(air, good_trace, rand_coeffs.AsSpan()),
      air.GetCompositionPolynomialDegreeBound());
  EXPECT_EQ(
      ComputeCompositionDegree(air, bad_trace, rand_coeffs.AsSpan()),
      2 * air.GetCompositionPolynomialDegreeBound() - 1);
}

TEST(AirTestUtils, DrawRandomTrace) {
  // Initialize test parameters.
  Prng prng;
  const size_t width = prng.UniformInt(0, 5);
  const size_t height = prng.UniformInt(0, 5);
  const Field field = Field::Create<FieldElementT>();
  const Trace trace1 = DrawRandomTrace(width, height, field, &prng);
  const Trace trace2 = DrawRandomTrace(width, height, field, &prng);

  EXPECT_EQ(trace1.Width(), width);
  EXPECT_EQ(trace2.Width(), width);

  for (size_t i = 0; i < width; i++) {
    ASSERT_EQ(trace1.GetColumn(i).Size(), height);
    ASSERT_EQ(trace2.GetColumn(i).Size(), height);
    if (height > 0) {
      ASSERT_NE(trace1.GetColumn(i), trace2.GetColumn(i));
    }
  }
}

TEST(AirTestUtils, FailingConstraintsTesting) {
  // Initialize test parameters.
  Prng prng;

  // Construct an AIR instance having 4 constraints, where constraints #0,2 are not satisfied by the
  // trace, and #1,3 are satisfied.
  // The provided trace is two columns, one random, and the other is the pointwise square of the
  // first column.
  const size_t trace_length = Pow2(5);
  DummyAir<FieldElementT> air(trace_length);
  air.n_constraints = 4;
  air.n_columns = 2;
  air.mask = {{{0, 0}, {0, 1}, {1, 0}}};

  air.composition_polynomial_degree_bound = 2 * trace_length;
  uint64_t constraint_degrees = 2 * trace_length - 2;

  air.point_exponents = {trace_length,  // Used to compute everywhere.
                         (*air.composition_polynomial_degree_bound - 1) -
                             (constraint_degrees +
                              /* nowhere */ 0 - /* everywhere */ trace_length)};
  air.constraints = {
      // Random constraint unlikely to be satisfied.
      [&prng](
          gsl::span<const FieldElementT> /*neighbors*/,
          gsl::span<const FieldElementT> /*periodic_columns*/,
          gsl::span<const FieldElementT> random_coefficients, const FieldElementT& /*point*/,
          gsl::span<const FieldElementT> /*gen_power*/,
          gsl::span<const FieldElementT> /*precomp_evals*/) {
        return FractionFieldElement<FieldElementT>(
            random_coefficients[0] * FieldElementT::RandomElement(&prng));
      },

      // Tests that the second column is the square of the first. Satisfied by construction.
      [](gsl::span<const FieldElementT> neighbors,
         gsl::span<const FieldElementT> /*periodic_columns*/,
         gsl::span<const FieldElementT> random_coefficients, const FieldElementT& /*point*/,
         gsl::span<const FieldElementT> /*gen_power*/,
         gsl::span<const FieldElementT> precomp_evals) {
        const FieldElementT constraint = (neighbors[0] * neighbors[0]) - neighbors[1];

        // Nowhere.
        const FieldElementT& numerator = FieldElementT::One();

        // Everywhere.
        const FieldElementT denominator = precomp_evals[0];

        return FractionFieldElement<FieldElementT>(
            constraint * random_coefficients[1] * numerator, denominator);
      },
      // Random constraint unlikely to be satisfied.
      [&prng](
          gsl::span<const FieldElementT> /*neighbors*/,
          gsl::span<const FieldElementT> /*periodic_columns*/,
          gsl::span<const FieldElementT> random_coefficients, const FieldElementT& /*point*/,
          gsl::span<const FieldElementT> /*gen_power*/,
          gsl::span<const FieldElementT> /*precomp_evals*/) {
        return FractionFieldElement<FieldElementT>(
            random_coefficients[2] * FieldElementT::RandomElement(&prng));
      },

      // Zero constrain, always Satisfied.
      [](gsl::span<const FieldElementT> /*neighbors*/,
         gsl::span<const FieldElementT> /*periodic_columns*/,
         gsl::span<const FieldElementT> /*random_coefficients*/, const FieldElementT& /*point*/,
         gsl::span<const FieldElementT> /*gen_power*/,
         gsl::span<const FieldElementT> /*precomp_evals*/) {
        return FractionFieldElement<FieldElementT>(FieldElementT::Zero());
      }};

  // Construct trace columns.
  const std::vector<FieldElementT> v_rand =
      prng.RandomFieldElementVector<FieldElementT>(trace_length);
  std::vector<FieldElementT> v_rand_sqr;
  v_rand_sqr.reserve(v_rand.size());
  for (const auto& e : v_rand) {
    v_rand_sqr.push_back(e * e);
  }

  // Construct trace.
  Trace trace(std::vector<std::vector<FieldElementT>>{v_rand, v_rand_sqr});

  // Verify.
  EXPECT_THAT(GetFailingConstraints(air, trace, &prng), UnorderedElementsAre(0, 2));
  EXPECT_FALSE(TestOneConstraint(air, trace, 0, &prng));
  EXPECT_TRUE(TestOneConstraint(air, trace, 1, &prng));
  EXPECT_FALSE(TestOneConstraint(air, trace, 2, &prng));
  EXPECT_TRUE(TestOneConstraint(air, trace, 3, &prng));
}

TEST(AirTestUtils, TestAirConstraint) {
  // Initialize test parameters.
  Prng prng;

  // Construct an AIR instance having 2 constraints:
  // first is unsatisfiable in any case, the second is satisfied in case
  // trace[0][0] + trace[1][0] + trace[0][1] = 0.
  const size_t trace_length = Pow2(5);
  DummyAir<FieldElementT> air(trace_length);
  air.n_constraints = 2;
  air.n_columns = 2;
  air.mask = {{{0, 0}, {0, 1}, {1, 0}}};

  air.composition_polynomial_degree_bound = trace_length;
  uint64_t constraint_degrees = trace_length - 1;

  air.point_exponents = {(*air.composition_polynomial_degree_bound - 1) -
                         (constraint_degrees +
                          /* nowhere */ 0 - /* first_line */ 1)};

  air.constraints = {
      // A function that is high degree unless random_coefficients[0] === random_coefficients[1]
      // ==0.
      // As this constraint is expected to be ignored, it does not matter what it does, notice it is
      // unsatisfiable.
      [&prng](
          gsl::span<const FieldElementT> /*neighbors*/,
          gsl::span<const FieldElementT> /*periodic_columns*/,
          gsl::span<const FieldElementT> random_coefficients, const FieldElementT& /*point*/,
          gsl::span<const FieldElementT> /*gen_power*/,
          gsl::span<const FieldElementT> /*precomp_evals*/) {
        return FractionFieldElement<FieldElementT>(
            random_coefficients[0] * FieldElementT::RandomElement(&prng));
      },
      // A constrain expecting the sum of the three neighbors is zero.
      [](gsl::span<const FieldElementT> neighbors,
         gsl::span<const FieldElementT> /*periodic_columns*/,
         gsl::span<const FieldElementT> random_coefficients, const FieldElementT& point,
         gsl::span<const FieldElementT> /*gen_power*/,
         gsl::span<const FieldElementT> /*precomp_evals*/) {
        const FieldElementT constraint = neighbors[0] + neighbors[1] + neighbors[2];

        // Nowhere.
        const FieldElementT& numerator = FieldElementT::One();

        // first_line
        const FieldElementT denominator = point - FieldElementT::One();

        return FractionFieldElement<FieldElementT>(
            constraint * random_coefficients[1] * numerator, denominator);
      }};

  // Test satisfiability of constraint #1.

  // It expects the sum of all mask elements to be zero.
  const auto trace_manipulator = [&](FieldElementSpan curr_row, FieldElementSpan next_row,
                                     bool make_satisfying) {
    next_row.Set(
        0, make_satisfying ? -(curr_row[0] + curr_row[1])
                           : FieldElement(FieldElementT::RandomElement(&prng)));
  };

  TestAirConstraint(
      air, Field::Create<FieldElementT>(), trace_length, /*constraint_idx=*/1,
      /*domain_indices=*/{0}, trace_manipulator, &prng);
}

TEST(AirTestUtils, DomainPredicateToList) {
  const auto predicate = [](const size_t i) { return (i % 5 == 0) || (i % 3 == 0); };

  EXPECT_EQ(
      DomainPredicateToList(predicate, 20), std::vector<size_t>({0, 3, 5, 6, 9, 10, 12, 15, 18}));
}

}  // namespace
}  // namespace starkware
