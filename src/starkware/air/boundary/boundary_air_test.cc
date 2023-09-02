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

#include "starkware/air/boundary/boundary_air.h"

#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/test_utils.h"
#include "starkware/algebra/domains/list_of_cosets.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/lde/lde.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/composition_polynomial/composition_polynomial.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/math/math.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {
namespace {

using FieldElementT = PrimeFieldElement<252, 0>;

/*
  This test builds a random trace. It then generate random points to sample on random columns, and
  creates boundary constraints for them. It then tests that the resulting composition polynomial is
  indeed of expected low degree., which means the constraints hold.
*/
TEST(BoundaryAir, Correctness) {
  Prng prng;

  const size_t n_columns = 10;
  const size_t n_conditions = 20;
  const uint64_t trace_length = 1024;
  std::vector<std::vector<FieldElementT>> trace;

  ListOfCosets domain(
      ListOfCosets::MakeListOfCosets(trace_length, 2, Field::Create<FieldElementT>()));

  auto lde_manager = MakeLdeManager(domain.Bases());

  // Generate random trace.
  trace.reserve(n_columns);
  for (size_t i = 0; i < n_columns; ++i) {
    trace.push_back(prng.RandomFieldElementVector<FieldElementT>(trace_length));
    lde_manager->AddEvaluation(FieldElementVector::CopyFrom(trace[i]));
  }

  // Compute correct boundary conditions.
  std::vector<std::tuple<size_t, FieldElement, FieldElement>> boundary_conditions;
  for (size_t condition_index = 0; condition_index < n_conditions; ++condition_index) {
    const size_t column_index = prng.UniformInt<size_t>(0, n_columns - 1);
    FieldElementVector points_x =
        FieldElementVector::Make(prng.RandomFieldElementVector<FieldElementT>(1));
    FieldElementVector points_y =
        FieldElementVector::MakeUninitialized(Field::Create<FieldElementT>(), 1);

    lde_manager->EvalAtPoints(column_index, points_x, points_y);
    boundary_conditions.emplace_back(
        column_index, points_x.As<FieldElementT>()[0], points_y.As<FieldElementT>()[0]);
  }

  BoundaryAir<FieldElementT> air(trace_length, n_columns, boundary_conditions);

  FieldElementVector random_coefficients = FieldElementVector::Make(
      prng.RandomFieldElementVector<FieldElementT>(air.NumRandomCoefficients()));

  const uint64_t actual_degree =
      ComputeCompositionDegree(air, Trace(std::move(trace)), random_coefficients);

  // Degree is expected to be trace_length - 2.
  EXPECT_EQ(trace_length - 2, actual_degree);
  EXPECT_EQ(air.GetCompositionPolynomialDegreeBound() - 2, actual_degree);
}

/*
  Similar to above test, only the boundary conditions are wrong, which means the resulting
  composition polynomial should be of high degree.
*/
TEST(BoundaryAir, Soundness) {
  Prng prng;

  const size_t n_columns = 10;
  const size_t n_conditions = 20;
  const uint64_t trace_length = 1024;
  std::vector<std::vector<FieldElementT>> trace;

  // Generate random trace.
  trace.reserve(n_columns);
  for (size_t i = 0; i < n_columns; ++i) {
    trace.push_back(prng.RandomFieldElementVector<FieldElementT>(trace_length));
  }

  // Compute incorrect boundary conditions.
  std::vector<std::tuple<size_t, FieldElement, FieldElement>> boundary_conditions;
  for (size_t condition_index = 0; condition_index < n_conditions; ++condition_index) {
    const size_t column_index = prng.UniformInt<size_t>(0, n_columns - 1);

    boundary_conditions.emplace_back(
        column_index, FieldElementT::RandomElement(&prng), FieldElementT::RandomElement(&prng));
  }

  BoundaryAir<FieldElementT> air(trace_length, n_columns, boundary_conditions);

  FieldElementVector random_coefficients = FieldElementVector::Make(
      prng.RandomFieldElementVector<FieldElementT>(air.NumRandomCoefficients()));

  const size_t num_of_cosets = 2;
  const uint64_t actual_degree =
      ComputeCompositionDegree(air, Trace(std::move(trace)), random_coefficients, num_of_cosets);

  EXPECT_EQ(num_of_cosets * air.GetCompositionPolynomialDegreeBound() - 1, actual_degree);
}

}  // namespace
}  // namespace starkware
