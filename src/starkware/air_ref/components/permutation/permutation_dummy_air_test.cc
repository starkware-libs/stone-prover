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

#include "starkware/air/components/permutation/permutation_dummy_air.h"

#include <memory>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/components/permutation/permutation_trace_context.h"
#include "starkware/air/test_utils.h"
#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/lde/lde.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using FieldElementT = TestFieldElement;
using AirT = PermutationDummyAir<FieldElementT, 0>;

class PermutationDummyAirTest : public ::testing::Test {
 public:
  const size_t trace_length = 256;
  Prng prng;
  AirT air;
  PermutationTraceContext<FieldElementT> trace_context;
  const size_t n_interaction_elements;
  std::optional<FieldElementVector> interaction_elms_vec;
  MaybeOwnedPtr<const Air> interaction_air;
  std::optional<Trace> merged_trace;
  std::optional<FieldElementVector> random_coefficients;

  PermutationDummyAirTest()
      : air(trace_length, &prng),
        trace_context(UseOwned(&air)),
        n_interaction_elements(air.GetInteractionParams()->n_interaction_elements),
        interaction_elms_vec(FieldElementVector::Make(
            prng.RandomFieldElementVector<TestFieldElement>(n_interaction_elements))) {}

  /*
    Generates first trace, interaction air and interaction trace. In case disrupt_interaction is set
    to true, it skips setting interaction elements.
  */
  void ComputeTracesAndInteractionAir(bool disrupt_interaction = false) {
    // Get the trace and the interaction trace.
    Trace first_trace = trace_context.GetTrace();
    if (!disrupt_interaction) {
      trace_context.SetInteractionElements(*interaction_elms_vec);
    }
    interaction_air = UseOwned(&(trace_context.GetAir()));

    // Generate final trace.
    std::vector<Trace> traces;
    traces.push_back(std::move(first_trace));
    traces.push_back(trace_context.GetInteractionTrace());

    merged_trace.emplace(MergeTraces<FieldElementT>(traces));

    random_coefficients = FieldElementVector::Make(
        prng.RandomFieldElementVector<TestFieldElement>(interaction_air->NumRandomCoefficients()));
  }
};

TEST_F(PermutationDummyAirTest, PositiveTest) {
  ComputeTracesAndInteractionAir();
  // Verify composition degree.
  EXPECT_LT(
      ComputeCompositionDegree(*interaction_air, *merged_trace, *random_coefficients),
      interaction_air->GetCompositionPolynomialDegreeBound());
}

TEST_F(PermutationDummyAirTest, NegativeTest) {
  ComputeTracesAndInteractionAir();
  // Choose a cell in the merged trace to ruin.
  const size_t bad_col_idx = prng.UniformInt<size_t>(0, merged_trace->Width() - 1);
  const size_t bad_row_idx = prng.UniformInt<size_t>(0, trace_length - 1);

  merged_trace->SetTraceElementForTesting(
      bad_col_idx, bad_row_idx, FieldElement(FieldElementT::RandomElement(&prng)));

  // Make sure the composition polynomial of the ruined merged trace has a high degree.
  EXPECT_GT(
      ComputeCompositionDegree(*interaction_air, *merged_trace, *random_coefficients),
      interaction_air->GetCompositionPolynomialDegreeBound() - 1);
}

TEST_F(PermutationDummyAirTest, IncompatibleInteractionElemsTest) {
  ComputeTracesAndInteractionAir();
  // Change interaction element in interaction air.
  interaction_elms_vec->Set(
      prng.UniformInt<size_t>(0, interaction_elms_vec->Size() - 1),
      FieldElement(FieldElementT::RandomElement(&prng)));
  trace_context.SetInteractionElementsForTest(*interaction_elms_vec);
  const Air& bad_interaction_air = trace_context.GetAir();
  // Make sure the composition polynomial of inconsistent interaction air has a high degree.
  EXPECT_GT(
      ComputeCompositionDegree(bad_interaction_air, *merged_trace, *random_coefficients),
      bad_interaction_air.GetCompositionPolynomialDegreeBound() - 1);
}

TEST_F(PermutationDummyAirTest, NegativeFunctionCalls) {
  ComputeTracesAndInteractionAir();
  // Call to trace_context.SetIntearctionElements twice.
  EXPECT_ASSERT(
      trace_context.SetInteractionElements(FieldElementVector::Make(
          prng.RandomFieldElementVector<TestFieldElement>(n_interaction_elements))),
      testing::HasSubstr("Interaction air was already set."));

  // Call to trace_context.GetInteractionTrace twice.
  EXPECT_ASSERT(
      trace_context.GetInteractionTrace(),
      testing::HasSubstr("Invalid call to GetInteractionTrace."));
}

TEST_F(PermutationDummyAirTest, GenerateInteractionTraceWithoutInteractionElements) {
  EXPECT_ASSERT(
      ComputeTracesAndInteractionAir(true),
      testing::HasSubstr("Invalid call to GetInteractionTrace."));
}

}  // namespace
}  // namespace starkware
