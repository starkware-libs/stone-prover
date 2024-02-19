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

#ifndef STARKWARE_AIR_BOUNDARY_BOUNDARY_AIR_H_
#define STARKWARE_AIR_BOUNDARY_BOUNDARY_AIR_H_

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/algebra/fields/fraction_field_element.h"
#include "starkware/algebra/utils/invoke_template_version.h"

namespace starkware {

/*
  A simple AIR the describes the contraints:
    (column_i(x) - y0_i) / (x - x0_i).
*/
template <typename FieldElementT>
class BoundaryAir : public Air {
 public:
  using FieldElementT_ = FieldElementT;
  using Builder = typename CompositionPolynomialImpl<BoundaryAir<FieldElementT>>::Builder;

  struct ConstraintData {
    size_t coeff_idx;
    size_t column_index;
    FieldElementT point_x;
    FieldElementT point_y;
  };

  /*
    Parameters:
    - trace_length, size of trace.
    - n_columns, number of columns in trace.
    - boundary_conditions, list of tuples (column, x, y) indicating the constraint that column(x)=y.
  */
  BoundaryAir(
      uint64_t trace_length, size_t n_columns,
      gsl::span<const std::tuple<size_t, FieldElement, FieldElement>> boundary_conditions)
      : Air(trace_length), trace_length_(trace_length), n_columns_(n_columns) {
    constraints_.reserve(boundary_conditions.size());
    size_t coeff_idx = 0;
    // Group constrains by the point_x store them in constraints_.
    for (const auto& [column_index, point_x, point_y] : boundary_conditions) {
      auto x = point_x.template As<FieldElementT>();

      // Insert the current boundary condition next to one with the same x or at the end of the
      // list.
      auto it = std::find_if(
          constraints_.begin(), constraints_.end(),
          [&x](const ConstraintData& constraint) { return constraint.point_x == x; });
      constraints_.insert(
          it, ConstraintData{coeff_idx, column_index, x, point_y.template As<FieldElementT>()});
      coeff_idx++;
    }
    // The mask touches each column once at the current row.
    mask_.reserve(n_columns_);
    for (size_t i = 0; i < n_columns_; ++i) {
      mask_.emplace_back(0, i);
    }
  }

  std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      const FieldElement& trace_generator,
      const ConstFieldElementSpan& random_coefficients) const override {
    Builder builder(0);
    FieldElementT gen = trace_generator.As<FieldElementT>();

    return builder.BuildUniquePtr(
        UseOwned(this), gen, TraceLength(), random_coefficients.As<FieldElementT>(), {}, {});
  };

  std::vector<std::vector<FieldElementT>> PrecomputeDomainEvalsOnCoset(
      const FieldElementT& /* point */, const FieldElementT& /* generator */,
      gsl::span<const uint64_t> /* point_exponents */,
      gsl::span<const FieldElementT> /* shifts */) const {
    return {};
  }

  auto ConstraintsEval(
      gsl::span<const FieldElementT> neighbors,
      gsl::span<const FieldElementT> /* periodic_columns */,
      gsl::span<const FieldElementT> random_coefficients,
      gsl::span<const FieldElementT> point_powers, gsl::span<const FieldElementT> /*shifts*/,
      gsl::span<const FieldElementT> /* precomp_domains */) const {
    ASSERT_DEBUG(neighbors.size() == n_columns_, "Wrong number of neighbors");
    ASSERT_DEBUG(
        random_coefficients.size() == constraints_.size(), "Wrong number of random coefficients");

    const FieldElementT& point = point_powers[0];

    FractionFieldElement<FieldElementT> outer_sum(FieldElementT::Zero());
    FieldElementT inner_sum(FieldElementT::Zero());

    FieldElementT prev_x = constraints_[0].point_x;

    for (const ConstraintData& constraint : constraints_) {
      const FieldElementT constraint_value =
          random_coefficients[constraint.coeff_idx] *
          (neighbors[constraint.column_index] - constraint.point_y);
      if (prev_x == constraint.point_x) {
        // All constraints with the same constraint.point_x are summed into inner_sum.
        inner_sum += constraint_value;
      } else {
        // New constraint.point_x, add the old (inner_sum/prev_x) to the outer_sum
        // and start a new inner_sum.
        outer_sum += FractionFieldElement<FieldElementT>(inner_sum, point - prev_x);
        inner_sum = constraint_value;
        prev_x = constraint.point_x;
      }
    }
    outer_sum += FractionFieldElement<FieldElementT>(inner_sum, point - prev_x);

    return outer_sum;
  }

  std::vector<FieldElementT> DomainEvalsAtPoint(
      gsl::span<const FieldElementT> /* point_powers */,
      gsl::span<const FieldElementT> /* shifts */) const {
    return {};
  }

  uint64_t TraceLength() const { return trace_length_; }

  uint64_t GetCompositionPolynomialDegreeBound() const override { return TraceLength(); };

  uint64_t NumRandomCoefficients() const override { return constraints_.size(); };

  std::vector<std::pair<int64_t, uint64_t>> GetMask() const override { return mask_; };

  uint64_t NumColumns() const override { return n_columns_; };

  std::optional<InteractionParams> GetInteractionParams() const override { return std::nullopt; }

 private:
  uint64_t trace_length_;
  size_t n_columns_;
  std::vector<ConstraintData> constraints_;
  std::vector<std::pair<int64_t, uint64_t>> mask_;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_BOUNDARY_BOUNDARY_AIR_H_
