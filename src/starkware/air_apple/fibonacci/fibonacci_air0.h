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
  "There is some Fibonacci sequence 1, w, (1+w), ... such that its fibonacci_claim_index-th element
  is claimed_fib".

  A Fibonacci trace has 2 columns: x, y.
  In the first row x_0 = 1, y_0 = w.
  And in the next rows x_{i+1} = y_i, y_{i+1} = x_i + y_i.
  After the fibonacci_claim_index-th row the last rows are the continuation of the fibonacci
  sequence.
*/

#ifndef STARKWARE_AIR_FIBONACCI_FIBONACCI_AIR0_H_
#define STARKWARE_AIR_FIBONACCI_FIBONACCI_AIR0_H_

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
class FibonacciAir<FieldElementT, 0> : public Air {
 public:
  using FieldElementT_ = FieldElementT;
  using Builder = typename CompositionPolynomialImpl<FibonacciAir>::Builder;

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

  std::optional<InteractionParams> GetInteractionParams() const override { return std::nullopt; }

  static constexpr uint64_t kConstraintDegree = 1;

  enum Columns {
    kXColumn,
    kYColumn,
    // Number of columns.
    kNumColumns,
  };

  enum PeriodicColumns {
    // Number of periodic columns.
    kNumPeriodicColumns,
  };

  enum Neighbors {
    kXRow0Neighbor,
    kXRow1Neighbor,
    kYRow0Neighbor,
    kYRow1Neighbor,
    // Number of neighbors.
    kNumNeighbors,
  };

  enum Constraints {
    kStateCopyCond,   // Constraint 0.
    kStepCond,        // Constraint 1.
    kInitXCond,       // Constraint 2.
    kVerifyResCond,   // Constraint 3.
    kNumConstraints,  // Number of constraints.
  };

 public:
  explicit FibonacciAir(
      uint64_t trace_length, uint64_t fibonacci_claim_index, const FieldElementT& claimed_fib)
      : Air(trace_length),
        fibonacci_claim_index_(fibonacci_claim_index),
        claimed_fib_(claimed_fib) {
    ASSERT_RELEASE(
        fibonacci_claim_index_ < trace_length,
        "fibonacci_claim_index must be smaller than trace_length.");
  }

  /*
    Generates the trace.
    witness is the w in the Fibonacci sequence 1, w, (1+w), ...
  */
  static Trace GetTrace(
      const FieldElementT& witness, uint64_t trace_length, uint64_t fibonacci_claim_index);

  static FieldElementT PublicInputFromPrivateInput(
      const FieldElementT& witness, uint64_t fibonacci_claim_index);

 private:
  /*
    The index of the requested Fibonacci element.
  */
  const uint64_t fibonacci_claim_index_;

  /*
    The value of the requested Fibonacci element.
  */
  const FieldElementT claimed_fib_;
};

}  // namespace starkware

#include "starkware/air/fibonacci/fibonacci_air0.inl"

#endif  // STARKWARE_AIR_FIBONACCI_FIBONACCI_AIR0_H_
