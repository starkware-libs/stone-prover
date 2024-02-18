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

#ifndef STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_DUMMY_AIR0_H_
#define STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_DUMMY_AIR0_H_

#include <memory>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/composition_polynomial/composition_polynomial.h"
#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"

namespace starkware {

template <typename FieldElementT>
class PermutationDummyAirDefinition<FieldElementT, 0> : public Air {
 public:
  using FieldElementT_ = FieldElementT;
  using Builder = typename CompositionPolynomialImpl<PermutationDummyAirDefinition>::Builder;

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

  std::optional<InteractionParams> GetInteractionParams() const override {
    InteractionParams interaction_params{kNumColumnsFirst, kNumColumnsSecond, 3};
    return interaction_params;
  }

  static constexpr uint64_t kNumColumnsFirst = 6;
  static constexpr uint64_t kNumColumnsSecond = 1;

  static constexpr uint64_t kConstraintDegree = 2;
  static constexpr uint64_t kNOriginalCols = 3;
  static constexpr uint64_t kNSeries = 1;

  enum Columns {
    kColumn0Column,
    kColumn1Column,
    kColumn2Column,
    kColumn3Column,
    kColumn4Column,
    kColumn5Column,
    kColumn6Inter1Column,
    // Number of columns.
    kNumColumns,
  };

  enum PeriodicColumns {
    // Number of periodic columns.
    kNumPeriodicColumns,
  };

  enum Neighbors {
    kColumn0Row0Neighbor,
    kColumn0Row1Neighbor,
    kColumn1Row0Neighbor,
    kColumn1Row1Neighbor,
    kColumn2Row0Neighbor,
    kColumn2Row1Neighbor,
    kColumn3Row0Neighbor,
    kColumn3Row1Neighbor,
    kColumn4Row0Neighbor,
    kColumn4Row1Neighbor,
    kColumn5Row0Neighbor,
    kColumn5Row1Neighbor,
    kColumn6Inter1Row0Neighbor,
    kColumn6Inter1Row1Neighbor,
    // Number of neighbors.
    kNumNeighbors,
  };

  enum Constraints {
    kMultiColumnPermPermInit0Cond,  // Constraint 0.
    kMultiColumnPermPermStep0Cond,  // Constraint 1.
    kMultiColumnPermPermLastCond,   // Constraint 2.
    kNumConstraints,                // Number of constraints.
  };

 public:
  explicit PermutationDummyAirDefinition(uint64_t trace_length) : Air(trace_length) {}

 protected:
  FieldElementT multi_column_perm__perm__interaction_elm_ = FieldElementT::Uninitialized();
  FieldElementT multi_column_perm__hash_interaction_elm0_ = FieldElementT::Uninitialized();
  FieldElementT multi_column_perm__hash_interaction_elm1_ = FieldElementT::Uninitialized();

  const FieldElementT multi_column_perm__perm__public_memory_prod_ = FieldElementT::One();
};

}  // namespace starkware

#include "starkware/air/components/permutation/permutation_dummy_air_definition0.inl"

#endif  // STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_DUMMY_AIR0_H_
