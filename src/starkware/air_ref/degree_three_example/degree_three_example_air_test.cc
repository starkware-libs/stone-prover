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

#include "starkware/air/degree_three_example/degree_three_example_air.h"

#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/test_utils.h"
#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/lde/lde.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using FieldElementT = TestFieldElement;

TEST(DegreeThreeExampleAirTest, CompositionEndToEnd) {
  using AirT = DegreeThreeExampleAir<FieldElementT>;

  Prng prng;
  const uint64_t trace_length = 512;
  const uint64_t res_claim_index = 500;
  const FieldElementT private_input(FieldElementT::RandomElement(&prng));
  const FieldElementT claimed_res(DegreeThreeExampleAir<FieldElementT>::PublicInputFromPrivateInput(
      private_input, res_claim_index));
  const AirT air(trace_length, res_claim_index, claimed_res);

  FieldElementVector random_coefficients = FieldElementVector::Make(
      prng.RandomFieldElementVector<FieldElementT>(air.NumRandomCoefficients()));

  // Construct trace.
  const Trace trace = AirT::GetTrace(private_input, trace_length, res_claim_index);

  // Verify composition degree.
  EXPECT_LT(
      ComputeCompositionDegree(air, trace, random_coefficients),
      air.GetCompositionPolynomialDegreeBound());

  // Negative case test.
  ASSERT_GT(trace.Width(), 0U);
  const Trace bad_trace = DrawRandomTrace(
      trace.Width(), trace.GetColumn(0).Size(), Field::Create<FieldElementT>(), &prng);

  // Verify composition degree.
  EXPECT_GT(
      ComputeCompositionDegree(air, bad_trace, random_coefficients),
      air.GetCompositionPolynomialDegreeBound() - 1);
}

}  // namespace
}  // namespace starkware
