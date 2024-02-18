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

/*
  Implements an AIR to the claim:
  "There is some sequence w, const * w**3 + periodic, const * prev**3 + periodic... such
  that its res_claim_index-th element is claimed_res".

  A degree_three_example trace has 1 column - x.
  In the first row x_0 = w.
  And in the next rows x_{i+1} = const * x_i**3 + periodic
  After the res_claim_index-th row the last rows are the continuation of the degree three sequence.
*/

#ifndef STARKWARE_AIR_DEGREE_THREE_EXAMPLE_DEGREE_THREE_EXAMPLE_AIR0_H_
#define STARKWARE_AIR_DEGREE_THREE_EXAMPLE_DEGREE_THREE_EXAMPLE_AIR0_H_

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/air.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/composition_polynomial/composition_polynomial.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"

namespace starkware {

template <typename FieldElementT>
class DegreeThreeExampleAir<FieldElementT, 0> : public Air {
 public:
  using FieldElementT_ = FieldElementT;
  using Builder = typename CompositionPolynomialImpl<DegreeThreeExampleAir>::Builder;

  std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      const FieldElement& trace_generator,
      const ConstFieldElementSpan& random_coefficients) const override;

  uint64_t GetCompositionPolynomialDegreeBound() const override {
    return kConstraintDegree * TraceLength();
  }

  std::vector<std::pair<int64_t, uint64_t>> GetMask() const override;

  uint64_t NumRandomCoefficients() const override { return kNumConstraints; }

  uint64_t NumColumns() const override { return kNumColumns; }

  std::vector<std::vector<FieldElementT>> PrecomputeDomainEvalsOnCoset(
      const FieldElementT& point, const FieldElementT& generator,
      gsl::span<const uint64_t> point_exponents, gsl::span<const FieldElementT> shifts) const;

  FractionFieldElement<FieldElementT> ConstraintsEval(
      gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
      gsl::span<const FieldElementT> random_coefficients, const FieldElementT& point,
      gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const;

  std::vector<FieldElementT> DomainEvalsAtPoint(
      gsl::span<const FieldElementT> point_powers, gsl::span<const FieldElementT> shifts) const;

  TraceGenerationContext GetTraceGenerationContext() const;

  void BuildPeriodicColumns(const FieldElementT& gen, Builder* builder) const;

  std::optional<InteractionParams> GetInteractionParams() const override { return std::nullopt; }

  static constexpr uint64_t kConstraintDegree = 4;

  enum Columns {
    kXColumn,
    // Number of columns.
    kNumColumns,
  };

  enum PeriodicColumns {
    kAddThreePeriodicColumn,
    // Number of periodic columns.
    kNumPeriodicColumns,
  };

  enum Neighbors {
    kXRow0Neighbor,
    kXRow1Neighbor,
    // Number of neighbors.
    kNumNeighbors,
  };

  enum Constraints {
    kStepCond,        // Constraint 0.
    kVerifyResCond,   // Constraint 1.
    kNumConstraints,  // Number of constraints.
  };

 public:
  explicit DegreeThreeExampleAir(
      uint64_t trace_length, uint64_t res_claim_index, const FieldElementT& claimed_res)
      : Air(trace_length), res_claim_index_(res_claim_index), claimed_res_(claimed_res) {
    ASSERT_RELEASE(
        res_claim_index_ < trace_length, "res_claim_index must be smaller than trace_length.");
  }

  /*
    Generates the trace.
    witness is the w in the DegreeThreeExample sequence w, const * w**3 + periodic,...
  */
  static Trace GetTrace(
      const FieldElementT& witness, uint64_t trace_length, uint64_t res_claim_index);

  static FieldElementT PublicInputFromPrivateInput(
      const FieldElementT& witness, uint64_t res_claim_index);

 private:
  /*
    The index of the requested element.
  */
  const uint64_t res_claim_index_;

  /*
    The value of the requested element.
  */
  const FieldElementT claimed_res_;

  /*
    The periodic column values in the air.
  */
  static const std::vector<FieldElementT> kValues;

  /*
    The constant in the air.
  */
  static const FieldElementT kCst;
};

}  // namespace starkware

#include "starkware/air/degree_three_example/degree_three_example_air0.inl"

#endif  // STARKWARE_AIR_DEGREE_THREE_EXAMPLE_DEGREE_THREE_EXAMPLE_AIR0_H_
