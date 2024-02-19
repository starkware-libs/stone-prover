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

#include <algorithm>

namespace starkware {

template <typename FieldElementT>
PermRangeCheckComponentProverContext1<FieldElementT>
PermRangeCheckComponentProverContext0<FieldElementT>::WriteTrace(
    gsl::span<const gsl::span<FieldElementT>> trace) && {
  auto data = std::move(range_check_cell_).Consume();
  ASSERT_RELEASE(
      data.size() == sorted_column_.Size(trace[0].size()), "Data size mismatches size of column");
  std::vector<uint64_t> sorted_values(data.begin(), data.end());
  std::sort(sorted_values.begin(), sorted_values.end());

  // Fill trace.
  sorted_column_.SetCell(trace, 0, FieldElementT::FromUint(sorted_values[0]));
  for (size_t i = 1; i < sorted_values.size(); ++i) {
    sorted_column_.SetCell(trace, i, FieldElementT::FromUint(sorted_values[i]));
    ASSERT_RELEASE(
        sorted_values[i] == sorted_values[i - 1] || sorted_values[i] == sorted_values[i - 1] + 1,
        "Missing range-check values between " + std::to_string(sorted_values[i - 1]) + " and " +
            std::to_string(sorted_values[i]));
  }

  return PermRangeCheckComponentProverContext1(
      std::move(perm_component_), sorted_values.front(), sorted_values.back(), std::move(data));
}

template <typename FieldElementT>
void PermRangeCheckComponentProverContext1<FieldElementT>::WriteTrace(
    const FieldElementT& interaction_elm,
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
      orig_spans, perm_spans, interaction_elm, interaction_trace, FieldElementT::One());
}

}  // namespace starkware
