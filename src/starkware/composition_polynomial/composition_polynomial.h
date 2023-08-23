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

#ifndef STARKWARE_COMPOSITION_POLYNOMIAL_COMPOSITION_POLYNOMIAL_H_
#define STARKWARE_COMPOSITION_POLYNOMIAL_COMPOSITION_POLYNOMIAL_H_

#include <functional>
#include <memory>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/polymorphic/field_element_span.h"
#include "starkware/composition_polynomial/multiplicative_neighbors.h"
#include "starkware/composition_polynomial/periodic_column.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

/*
  Represents a polynomial of the form:

  F(x, y_1, y_2, ... , y_k) =
  \sum_i c_i * f_i(x, y_0, y_1, ... , y_k, p_0, ..., p_m) *
  P_i(x)/Q_i(x).

  Where:

  - The sequence (p_0, ..., p_m) consists of the evaluations of the periodic public columns.
  - The term f_i(y_0, y_1, ... , y_k, p_0, ..., p_m) represents a constraint.
  - The term P_i(x)/Q_i(x) is rational function such that Q_i(x)/P(i) is a polynomial with only
  simple roots, and it's roots are exactly the locations the constraint f_i has to be satisfied on.

  Parameters deduction:

  - (c_0, c_1, ... ) are the 'coefficients'.
  - The functions (f_0, f_1,...) are induced by air.ConstraintsEval().
  - The mask (for evaluation on entire cosets) is obtained from air.GetMask().

  This class is used both to evaluate F( x, y_0, y_1, ...) on a single point, and on entire cosets
  using optimizations improving the (amortized) computation time for each point in the coset.

*/
class CompositionPolynomial {
 public:
  virtual ~CompositionPolynomial() = default;

  /*
    Evaluates the polynomial on a single point. The neighbors are the values obtained from the trace
    low degree extension, using the AIR's mask.
  */
  virtual FieldElement EvalAtPoint(
      const FieldElement& point, const ConstFieldElementSpan& neighbors) const = 0;

  /*
    Evaluates composition_poly on the coset coset_offset*<group_genertor>, which must be of size
    coset_size. The evaluation is split to different tasks of size task_size each. The evaluation
    is written to 'out_evaluation', in bit reversed order: out_evaluation[i] contains the evaluation
    on the point
      coset_offset*(group_genertor^{bit_reverse(i)}).
  */
  virtual void EvalOnCosetBitReversedOutput(
      const FieldElement& coset_offset, gsl::span<const ConstFieldElementSpan> trace_lde,
      const FieldElementSpan& out_evaluation, uint64_t task_size) const = 0;

  virtual uint64_t GetDegreeBound() const = 0;
};

template <typename AirT>
class CompositionPolynomialImpl : public CompositionPolynomial {
 public:
  using FieldElementT = typename AirT::FieldElementT_;

  class Builder {
   public:
    explicit Builder(uint64_t num_periodic_columns) : periodic_columns_(num_periodic_columns) {}

    void AddPeriodicColumn(PeriodicColumn<FieldElementT> column, size_t periodic_column_index);

    /*
      Builds an instance of CompositionPolynomialImpl.
      Note that once Build or BuildUniquePtr are used, the constraints that were added
      previously are consumed and the Builder goes back to a clean slate state.
    */
    CompositionPolynomialImpl Build(
        MaybeOwnedPtr<const AirT> air, const FieldElementT& trace_generator, uint64_t coset_size,
        gsl::span<const FieldElementT> random_coefficients,
        gsl::span<const uint64_t> point_exponents, gsl::span<const FieldElementT> shifts);

    std::unique_ptr<CompositionPolynomialImpl<AirT>> BuildUniquePtr(
        MaybeOwnedPtr<const AirT> air, const FieldElementT& trace_generator, uint64_t coset_size,
        gsl::span<const FieldElementT> random_coefficients,
        gsl::span<const uint64_t> point_exponents, gsl::span<const FieldElementT> shifts);

   private:
    std::vector<uint64_t> constraint_degrees_;
    std::vector<std::optional<PeriodicColumn<FieldElementT>>> periodic_columns_;
  };

  FieldElement EvalAtPoint(
      const FieldElement& point, const ConstFieldElementSpan& neighbors) const override;

  FieldElementT EvalAtPoint(
      const FieldElementT& point, gsl::span<const FieldElementT> neighbors) const;

  void EvalOnCosetBitReversedOutput(
      const FieldElement& coset_offset, gsl::span<const ConstFieldElementSpan> trace_lde,
      const FieldElementSpan& out_evaluation, uint64_t task_size) const override;

  void EvalOnCosetBitReversedOutput(
      const FieldElementT& coset_offset,
      const MultiplicativeNeighbors<FieldElementT>& multiplicative_neighbors,
      gsl::span<FieldElementT> out_evaluation, uint64_t task_size) const;

  uint64_t GetDegreeBound() const override { return air_->GetCompositionPolynomialDegreeBound(); }

 private:
  /*
    The constructor is private.
    Users should use the Builder class to build an instance of this class.
  */
  CompositionPolynomialImpl(
      MaybeOwnedPtr<const AirT> air, FieldElementT trace_generator, uint64_t coset_size,
      std::vector<PeriodicColumn<FieldElementT>> periodic_columns,
      gsl::span<const FieldElementT> coefficients, gsl::span<const uint64_t> point_exponents,
      gsl::span<const FieldElementT> shifts);

  /*
    Returns the inverse of all denominators needed for evaluation over the coset
    coset_offset*<group_generator>, which is expected to be of size group_size. n_points
    is number of points on which the function operates. Technically, it return a vector of length
    group_size*n_constraints where n_constraints is the number of constraints and the denominator of
    the i-th constraint needed for the point coset_offset*group_generator^j is stored at index
    n_constraints*j + i in the result.
  */
  void ComputeDenominatorsInv(
      const FieldElementT& offset, uint64_t n_points,
      gsl::span<FieldElementT> denominators_inv) const;

  MaybeOwnedPtr<const AirT> air_;
  FieldElementT trace_generator_;
  uint64_t coset_size_;
  std::vector<PeriodicColumn<FieldElementT>> periodic_columns_;
  const std::vector<FieldElementT> coefficients_;
  const std::vector<uint64_t> point_exponents_;
  const std::vector<FieldElementT> shifts_;
};

}  // namespace starkware

#include "starkware/composition_polynomial/composition_polynomial.inl"

#endif  // STARKWARE_COMPOSITION_POLYNOMIAL_COMPOSITION_POLYNOMIAL_H_
