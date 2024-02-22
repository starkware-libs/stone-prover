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

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

template <typename FieldElementT>
DilutedCheckComponentProverContext1<FieldElementT>
DilutedCheckComponentProverContext0<FieldElementT>::WriteTrace(
    gsl::span<const gsl::span<FieldElementT>> trace) && {
  auto data = std::move(diluted_check_cell_).Consume();
  ASSERT_RELEASE(
      data.size() == sorted_column_.Size(trace[0].size()), "Data size mismatches size of column");
  std::vector<uint64_t> sorted_values(data.begin(), data.end());
  std::sort(sorted_values.begin(), sorted_values.end());

  // Fill trace.
  ASSERT_RELEASE(
      sorted_values[0] == 0,
      "Missing diluted-check values up to " + std::to_string(sorted_values[0]));
  sorted_column_.SetCell(trace, 0, FieldElementT::Zero());
  for (size_t i = 1; i < sorted_values.size(); ++i) {
    sorted_column_.SetCell(trace, i, FieldElementT::FromUint(sorted_values[i]));
    ASSERT_RELEASE(
        sorted_values[i] == sorted_values[i - 1] ||
            sorted_values[i] ==
                diluted_check_cell::Dilute(
                    diluted_check_cell::Undilute(sorted_values[i - 1], spacing_, n_bits_) + 1,
                    spacing_, n_bits_),
        "Missing diluted-check values between " + std::to_string(sorted_values[i - 1]) + " and " +
            std::to_string(sorted_values[i]));
  }

  return DilutedCheckComponentProverContext1(
      spacing_, n_bits_, std::move(perm_component_), std::move(data), std::move(cum_val_col_));
}

template <typename FieldElementT>
void DilutedCheckComponentProverContext1<FieldElementT>::WriteTrace(
    FieldElementT perm_interaction_elm, FieldElementT interaction_z,
    FieldElementT interaction_alpha,
    gsl::span<const gsl::span<FieldElementT>> interaction_trace) const {
  // Sort again, to avoid holding data in memory.
  // If this becomes a bottleneck, we should just save it in memory.
  std::vector<uint64_t> sorted_values(data_.begin(), data_.end());
  std::sort(sorted_values.begin(), sorted_values.end());

  // Cast data_ to field elements.
  std::vector<FieldElementT> elements;
  elements.reserve(data_.size());
  for (const uint64_t value : data_) {
    elements.push_back(FieldElementT::FromUint(value));
  }

  // Cast sorted_values to field elements.
  std::vector<FieldElementT> sorted_elements;
  sorted_elements.reserve(sorted_values.size());
  for (const uint64_t value : sorted_values) {
    sorted_elements.push_back(FieldElementT::FromUint(value));
  }

  std::vector<gsl::span<const FieldElementT>> orig_spans = {elements};
  std::vector<gsl::span<const FieldElementT>> perm_spans = {sorted_elements};

  perm_component_.WriteInteractionTrace(
      orig_spans, perm_spans, perm_interaction_elm, interaction_trace, FieldElementT::One());

  // Value in cell i of interaction column cumulative_value is
  // cumulative_value[i-1] * (1 + interaction_z * diff) + interaction_alpha * diff^2
  // where diff = sorted_elements[i] - sorted_elements[i-1]
  auto val = FieldElementT::One();
  cum_val_col_.SetCell(interaction_trace, 0, val);
  for (size_t i = 1; i < sorted_elements.size(); ++i) {
    const FieldElementT diff = sorted_elements[i] - sorted_elements[i - 1];
    val *= FieldElementT::One() + interaction_z * diff;
    val += interaction_alpha * diff * diff;
    cum_val_col_.SetCell(interaction_trace, i, val);
  }

  // Check that the last value in the cum_prod column is indeed as expected.
  const FieldElementT expected =
      ExpectedFinalCumulativeValue(spacing_, n_bits_, interaction_z, interaction_alpha);
  ASSERT_RELEASE(
      val == expected, "Last value in cumulative_value column is wrong. Expected: " +
                           expected.ToString() + " , actual: " + val.ToString() + ".");
}

template <typename FieldElementT>
FieldElementT DilutedCheckComponentProverContext1<FieldElementT>::ExpectedFinalCumulativeValue(
    size_t spacing, size_t n_bits, const FieldElementT& interaction_z,
    const FieldElementT& interaction_alpha) {
  // The cumulative value is defined using the next recursive formula:
  //   r_1 = 1, r_{j+1} = r_j * (1 + z * u_j) + alpha * u_j^2
  // where u_j = Dilute(j, spacing, n_bits) - Dilute(j-1, spacing, n_bits)
  // and we want to compute the final value r_{2^n_bits}.
  // Note that u_j depends only on the number of trailing zeros in the binary representation of j.
  // Specifically, u_{(1+2k)*2^i} = u_{2^i} = u_{2^{i-1}} + 2^{i*spacing} - 2^{(i-1)*spacing + 1}.
  //
  // The recursive formula can be reduced to a nonrecursive form:
  //   r_j = prod_{n=1..j-1}(1+z*u_n) + alpha*sum_{n=1..j-1}(u_n^2 * prod_{m=n+1..j-1}(1+z*u_m))
  //
  // We rewrite this equation to generate a recursive formula that converges in log(j) steps:
  // Denote:
  //   p_i = prod_{n=1..2^i-1}(1+z*u_n)
  //   q_i = sum_{n=1..2^i-1}(u_n^2 * prod_{m=n+1..2^i-1}(1+z*u_m))
  //   x_i = u_{2^i}.
  //
  // Clearly
  //   r_{2^i} = p_i + alpha * q_i.
  // Moreover,
  //   p_i = p_{i-1} * (1 + z * x_{i-1}) * p_{i-1}
  //   q_i = q_{i-1} * (1 + z * x_{i-1}) * p_{i-1} + x_{i-1}^2 * p_{i-1} + q_{i-1}
  //
  // Now we can compute p_{n_bits} and q_{n_bits} in just n_bits recursive steps and we are done.
  ASSERT_RELEASE(
      spacing < 64, "ExpectedFinalCumulativeValue is not implemented for large spacing.");
  FieldElementT p = FieldElementT::One() + interaction_z;
  FieldElementT q = FieldElementT::One();
  const FieldElementT diff_multiplier = FieldElementT::FromUint(uint64_t(1) << spacing);
  FieldElementT diff_x = diff_multiplier - FieldElementT::One() - FieldElementT::One();
  FieldElementT x = FieldElementT::One();

  for (size_t i = 1; i < n_bits; ++i) {
    x += diff_x;
    diff_x *= diff_multiplier;
    // To save multiplications, store intermediate values.
    const FieldElementT x_p = x * p;
    const FieldElementT y = p + interaction_z * x_p;
    q = q * y + x * x_p + q;
    p *= y;
  }
  return p + q * interaction_alpha;
}

}  // namespace starkware
