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
size_t ParseToDilutedComponent<FieldElementT>::RowIndex(size_t m) const {
  size_t row = 0;
  for (size_t i = 0; i < extended_dimension_sizes_.size(); ++i) {
    row *= dimension_capacities_[i];
    row += m % extended_dimension_sizes_[i];
    m /= extended_dimension_sizes_[i];
  }
  return row;
}

template <typename FieldElementT>
void ParseToDilutedComponent<FieldElementT>::WriteTrace(
    gsl::span<const FieldElementT> input,
    gsl::span<const TableCheckCellView<FieldElementT>> diluted_columns, uint64_t component_index,
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  ASSERT_RELEASE(input.size() == n_instances_ * n_repetitions_ * n_words_, "Invalid input size.");
  const FieldElementT spacing_shift = FieldElementT::FromUint(Pow2(diluted_spacing_));
  const auto two = FieldElementT::FromUint(2);
  for (size_t rep = 0; rep < n_repetitions_; ++rep) {
    for (size_t instance = 0; instance < n_instances_; ++instance) {
      for (size_t index = n_total_bits_; index < extended_dimensions_total_size_; ++index) {
        // Nullify all the trace cells that represent indices larger than the parsed area. They are
        // used as margins to the differential constraints.
        cumulative_sum_column_.SetCell(
            trace,
            rep + n_repetitions_ * (instance + n_instances_ * (RowIndex(index) +
                                                               total_period_ * component_index)),
            FieldElementT::Zero());
      }
      auto single_column_cumulative_sum = FieldElementT::Zero();
      FieldElementT prev_value = FieldElementT::Zero();
      size_t bit_index = n_total_bits_ - 1;
      for (int64_t i = n_words_ - 1; i >= 0; --i) {
        const FieldElementT& current_input =
            input[i + n_words_ * (rep + n_repetitions_ * instance)];
        intermediate_column_.SetCell(
            trace,
            i + n_words_ * (rep + n_repetitions_ * total_period_ *
                                      (instance + n_instances_ * component_index)),
            current_input);
        final_column_.SetCell(
            trace,
            i + n_words_ * (rep + n_repetitions_ *
                                      (instance + n_instances_ * total_period_ * component_index)),
            current_input);

        const std::vector<bool> bits = current_input.ToStandardForm().ToBoolVector();
        for (int64_t j = state_rep_[i] - 1; j >= 0; --j, --bit_index) {
          single_column_cumulative_sum *= two;
          if (bits[j]) {
            single_column_cumulative_sum += FieldElementT::One();
          }
          FieldElementT value = single_column_cumulative_sum;
          const size_t row_bit_index = RowIndex(bit_index);
          if (instance > 0) {
            value +=
                spacing_shift *
                trace[cumulative_sum_column_.column][cumulative_sum_column_.ToTraceRowIndex(
                    rep + n_repetitions_ *
                              (instance - 1 +
                               n_instances_ * (row_bit_index + total_period_ * component_index)))];
          }
          cumulative_sum_column_.SetCell(
              trace,
              rep + n_repetitions_ * (instance + n_instances_ * (row_bit_index +
                                                                 total_period_ * component_index)),
              value);
          if (instance == n_instances_ - 1) {
            diluted_columns[rep].WriteTrace(
                row_bit_index + total_period_ * component_index,
                (value - two * prev_value).ToStandardForm().AsUint(), trace);
            prev_value = value;
          }
        }
      }
    }
  }
}

}  // namespace starkware
