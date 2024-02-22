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

#ifndef STARKWARE_AIR_COMPONENTS_EC_SUBSET_SUM_EC_SUBSET_SUM_H_
#define STARKWARE_AIR_COMPONENTS_EC_SUBSET_SUM_EC_SUBSET_SUM_H_

#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/bit_unpacking/bit_unpacking.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"
#include "starkware/algebra/elliptic_curve/elliptic_curve.h"
#include "starkware/algebra/fields/fraction_field_element.h"

namespace starkware {

template <typename FieldElementT, size_t LimitLimbs = FieldElementT::ValueType::kN>
class EcSubsetSumComponent {
  using BitUnpackingComponentT = BitUnpackingComponent<FieldElementT, LimitLimbs>;

 public:
  EcSubsetSumComponent(
      const std::string& name, const TraceGenerationContext& ctx, uint64_t component_height,
      uint64_t n_points, bool use_x_diff_inv, bool use_bit_unpacking)
      : component_height_(component_height),
        n_points_(n_points),
        partial_sum_x_(ctx.GetVirtualColumn(name + "/partial_sum/x")),
        partial_sum_y_(ctx.GetVirtualColumn(name + "/partial_sum/y")),
        slope_(ctx.GetVirtualColumn(name + "/slope")),
        selector_(ctx.GetVirtualColumn(name + "/selector")),
        x_diff_inv_(
            use_x_diff_inv ? ctx.GetVirtualColumn(name + "/x_diff_inv")
                           : std::optional<VirtualColumn>()),
        bit_unpacking_component_(
            use_bit_unpacking
                ? std::make_optional<BitUnpackingComponentT>(name + "/bit_unpacking", ctx)
                : std::nullopt) {
    ASSERT_RELEASE(
        n_points < component_height, "Number of points does not fit into component height.");
    ASSERT_RELEASE(
        component_height - n_points < FieldElementT::FieldSize().Log2Floor(),
        "Too long tail. Component will not ensure that selector is padded with zeroes.");
    ASSERT_RELEASE(
        partial_sum_y_.view.step == partial_sum_y_.view.step, "Inconsistent step value.");
    ASSERT_RELEASE(slope_.view.step == partial_sum_y_.view.step, "Inconsistent step value.");
    ASSERT_RELEASE(selector_.view.step == partial_sum_y_.view.step, "Inconsistent step value.");
    if (x_diff_inv_.has_value()) {
      ASSERT_RELEASE(
          x_diff_inv_->view.step == partial_sum_y_.view.step, "Inconsistent step value.");
    }
  }

  /*
    Writes the trace for one instance of the component.
    shift_point - the value initializing the partial sum.
    points - the values of the points of the subset sum (span of length at least n_points_).
    component_index - the index of the component instance
    Does not fill slope and x_diff_inv in the last row of the component.
  */
  EcPoint<FieldElementT> WriteTrace(
      const EcPoint<FieldElementT>& shift_point, gsl::span<const EcPoint<FieldElementT>> points,
      const FieldElementT& selector_value, uint64_t component_index,
      gsl::span<const gsl::span<FieldElementT>> trace) const;

  /*
    Given constant points and a shift point on an EC and a vector of bits (selector_value), the
    function returns a point on the EC which is the linear combinations of the points, the bit
    vector and the shift point.
  */
  static EcPoint<FieldElementT> Hash(
      const EcPoint<FieldElementT>& shift_point, gsl::span<const EcPoint<FieldElementT>> points,
      const FieldElementT& selector_value);

 private:
  /*
    The period of the component (inside the virtual column).
  */
  const uint64_t component_height_;

  /*
    The number of points in the subset sum.
  */
  const uint64_t n_points_;

  /*
    Elliptic curve point column representing the partial sum.
  */
  const VirtualColumn partial_sum_x_;
  const VirtualColumn partial_sum_y_;

  /*
    A virtual column, needed for the computation of the group operation.
  */
  const VirtualColumn slope_;

  /*
    A virtual column, binary(selector_0) are the bits that select the subset.
  */
  const VirtualColumn selector_;

  /*
    A virtual column, inverse of (partial_sum.x - points.x), to prove that they are different.
  */
  const std::optional<VirtualColumn> x_diff_inv_;

  /*
    The bit unpacking component that enforces a limit on the selector.
  */
  const std::optional<BitUnpackingComponentT> bit_unpacking_component_;

  /*
    Columns for fractional field elements sub-trace instance.
  */
  enum FFTraceCols { kPartialSumX, kPartialSumY, kSlope, kXDiffInv, kNumCols };

  /*
    Given a starting point, shift_point, and a list of ECPoint<FieldElementsT> points, converts them
    to fraction field elements of the field and calculates an instance of the ec_subset_sum trace.
    Writes the result into ff_trace. The reason for converting the field elements to fractional
    field elements is to enable efficient computation of inverse for the elements.
  */
  void GetFractionFieldTraceInstance(
      const EcPoint<FieldElementT>& shift_point, gsl::span<const EcPoint<FieldElementT>> points,
      const std::vector<bool>& selector_bits,
      gsl::span<const gsl::span<FractionFieldElement<FieldElementT>>> ff_trace) const;
};

}  // namespace starkware

#include "starkware/air/components/ec_subset_sum/ec_subset_sum.inl"

#endif  // STARKWARE_AIR_COMPONENTS_EC_SUBSET_SUM_EC_SUBSET_SUM_H_
