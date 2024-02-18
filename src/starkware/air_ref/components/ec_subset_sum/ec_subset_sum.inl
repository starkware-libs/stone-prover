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

#include <vector>

namespace starkware {

template <typename FieldElementT, size_t LimitLimbs>
void EcSubsetSumComponent<FieldElementT, LimitLimbs>::GetFractionFieldTraceInstance(
    const EcPoint<FieldElementT>& shift_point, const gsl::span<const EcPoint<FieldElementT>> points,
    const std::vector<bool>& selector_bits,
    gsl::span<const gsl::span<FractionFieldElement<FieldElementT>>> ff_trace) const {
  using FractionFieldElementT = FractionFieldElement<FieldElementT>;

  ASSERT_RELEASE(
      points.size() == n_points_ || points.size() == component_height_,
      "Number of points should be either n_points, or component_height.");

  // Set partial_sum_x and partial_sum_y columns at the first row.
  EcPoint<FractionFieldElementT> partial_sum(
      FractionFieldElementT(shift_point.x), FractionFieldElementT(shift_point.y));
  ff_trace[kPartialSumX][0] = partial_sum.x;
  ff_trace[kPartialSumY][0] = partial_sum.y;

  // Fill in the rest of the partial ff_trace.
  for (uint64_t j = 0; j < component_height_ - 1; j++) {
    const bool selector_bit = j < selector_bits.size() && selector_bits[j];
    const size_t point_index = j < points.size() ? j : n_points_ - 1;
    EcPoint<FractionFieldElementT> cur_point{FractionFieldElementT(points[point_index].x),
                                             FractionFieldElementT(points[point_index].y)};

    // Set x_diff_inv.
    ASSERT_RELEASE(
        partial_sum.x != cur_point.x, "Adding a point to itself or to its inverse point.");
    if (x_diff_inv_) {
      ff_trace[kXDiffInv][j] = (partial_sum.x - cur_point.x).Inverse();
    }

    // Set slope.
    FractionFieldElementT slope = GetSlope(partial_sum, cur_point);
    ff_trace[kSlope][j] = selector_bit ? slope : FractionFieldElementT::Zero();

    // Set next partial_sum.
    if (selector_bit) {
      ASSERT_RELEASE(j < n_points_, "Given selector is too big.");
      partial_sum = AddPointsGivenSlope(partial_sum, cur_point, slope);
    }

    ff_trace[kPartialSumX][j + 1] = partial_sum.x;
    ff_trace[kPartialSumY][j + 1] = partial_sum.y;
  }
}

template <typename FieldElementT, size_t LimitLimbs>
EcPoint<FieldElementT> EcSubsetSumComponent<FieldElementT, LimitLimbs>::WriteTrace(
    const EcPoint<FieldElementT>& shift_point, const gsl::span<const EcPoint<FieldElementT>> points,
    const FieldElementT& selector_value, uint64_t component_index,
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  using FractionFieldElementT = FractionFieldElement<FieldElementT>;

  const uint64_t row_offset = component_index * component_height_;

  auto selector_value_as_big_int = selector_value.ToStandardForm();
  const std::vector<bool> selector_bits = selector_value_as_big_int.ToBoolVector();

  ASSERT_RELEASE(
      selector_value_as_big_int == selector_value_as_big_int.Zero() ||
          selector_value_as_big_int.Log2Floor() < n_points_,
      "Given selector is too big.");

  // Create a part of the trace for the current component with fractional field elements.
  thread_local std::vector<std::vector<FractionFieldElementT>> ff_trace(kNumCols);
  for (auto& vec : ff_trace) {
    vec.resize(component_height_, FractionFieldElementT::Zero());
  }

  EcSubsetSumComponent<FieldElementT, LimitLimbs>::GetFractionFieldTraceInstance(
      shift_point, points, selector_bits,
      std::vector<gsl::span<FractionFieldElementT>>(ff_trace.begin(), ff_trace.end()));

  // Convert ff_trace to base_from_ff_trace: base_from_ff_trace[i][j] is the base field element of
  // ff_trace[i][j].
  thread_local std::vector<std::vector<FieldElementT>> base_from_ff_trace(kNumCols);
  for (auto& vec : base_from_ff_trace) {
    vec.resize(component_height_, FieldElementT::Zero());
  }

  FractionFieldElementT::BatchToBaseFieldElement(
      std::vector<gsl::span<const FractionFieldElementT>>(ff_trace.begin(), ff_trace.end()),
      std::vector<gsl::span<FieldElementT>>(base_from_ff_trace.begin(), base_from_ff_trace.end()));

  // Write bit unpacking trace.
  if (bit_unpacking_component_.has_value()) {
    bit_unpacking_component_->WriteTrace(
        component_index, trace, BigInt<LimitLimbs>::FromBigInt(selector_value_as_big_int));
  }

  // Fill trace for the current component with base_from_ff_trace elements.
  for (uint64_t j = 0; j < component_height_ - 1; j++) {
    uint64_t row = row_offset + j;
    partial_sum_x_.SetCell(trace, row, base_from_ff_trace[kPartialSumX][j]);
    partial_sum_y_.SetCell(trace, row, base_from_ff_trace[kPartialSumY][j]);

    // Set x_diff_inv.
    if (x_diff_inv_) {
      x_diff_inv_->SetCell(trace, row, base_from_ff_trace[kXDiffInv][j]);
    }

    // Set selector.
    selector_.SetCell(trace, row, FieldElementT::FromBigInt(selector_value_as_big_int));
    selector_value_as_big_int >>= 1;

    // Set slope.
    slope_.SetCell(trace, row, base_from_ff_trace[kSlope][j]);
  }

  uint64_t last_row = row_offset + component_height_ - 1;
  // Set last selector.
  selector_.SetCell(trace, last_row, FieldElementT::Zero());

  EcPoint<FieldElementT> final_sum(
      base_from_ff_trace[kPartialSumX][component_height_ - 1],
      base_from_ff_trace[kPartialSumY][component_height_ - 1]);
  partial_sum_x_.SetCell(trace, last_row, final_sum.x);
  partial_sum_y_.SetCell(trace, last_row, final_sum.y);
  return final_sum;
}

template <typename FieldElementT, size_t LimitLimbs>
EcPoint<FieldElementT> EcSubsetSumComponent<FieldElementT, LimitLimbs>::Hash(
    const EcPoint<FieldElementT>& shift_point, gsl::span<const EcPoint<FieldElementT>> points,
    const FieldElementT& selector_value) {
  using FractionFieldElementT = FractionFieldElement<FieldElementT>;
  auto partial_sum = shift_point.template ConvertTo<FractionFieldElementT>();
  const auto selector_value_as_big_int = selector_value.ToStandardForm();
  const std::vector<bool> selector_bits = selector_value_as_big_int.ToBoolVector();
  ASSERT_RELEASE(
      selector_value_as_big_int == selector_value_as_big_int.Zero() ||
          selector_value_as_big_int.Log2Floor() < points.size(),
      "Given selector is too big.");
  ASSERT_RELEASE(points.size() <= selector_bits.size(), "Too many points.");

  for (size_t j = 0; j < points.size(); j++) {
    const auto point = points[j].template ConvertTo<FractionFieldElementT>();
    ASSERT_RELEASE(partial_sum.x != point.x, "Adding a point to itself or to its inverse point.");
    if (selector_bits[j]) {
      partial_sum = partial_sum + point;
    }
  }
  return partial_sum.template ConvertTo<FieldElementT>();
}

}  // namespace starkware
