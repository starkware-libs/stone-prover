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

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

/*
  Given a span of address value pairs, sorts them according to address in ascending order.
  We sort the value pointers instead of the actual values to avoid memory consumption.
*/
template <typename FieldElementT>
std::vector<std::pair<uint64_t, const FieldElementT*>> AddressBasedSort(
    gsl::span<const uint64_t> address, gsl::span<const FieldElementT> value) {
  std::vector<std::pair<uint64_t, const FieldElementT*>> sorted_address_value;
  ASSERT_RELEASE(address.size() == value.size(), "Address and value have different sizes.");
  sorted_address_value.reserve(address.size());
  for (size_t i = 0; i < address.size(); ++i) {
    sorted_address_value.emplace_back(address[i], &value[i]);
  }
  std::sort(
      sorted_address_value.begin(), sorted_address_value.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

  return sorted_address_value;
}

/*
  Checks that first_cell and second_cell are valid consecutive memory cells.
*/
template <typename FieldElementT>
void ValidateConsecutiveCells(
    const std::pair<uint64_t, FieldElementT*>& first_cell,
    const std::pair<uint64_t, FieldElementT*>& second_cell, const size_t index) {
  bool same_address =
      first_cell.first == second_cell.first && *first_cell.second == *second_cell.second;
  bool continuous_address = second_cell.first == first_cell.first + 1;

  ASSERT_RELEASE(
      same_address || continuous_address,
      "Problem with memory in row number " + std::to_string(index) + ". Addresses: " +
          std::to_string(first_cell.first) + " and " + std::to_string(second_cell.first) +
          ", values: " + first_cell.second->ToString() + " and " + second_cell.second->ToString());
}

template <typename FieldElementT>
MemoryComponentProverContext1<FieldElementT>
MemoryComponentProverContext<FieldElementT>::WriteTrace(
    gsl::span<const gsl::span<FieldElementT>> trace, bool disable_asserts) && {
  // NOLINTNEXTLINE (auto [...]).
  auto [address, value, public_memory_indices] = std::move(memory_cell_).Consume();
  if (!disable_asserts) {
    ASSERT_RELEASE(!trace.empty(), "Trace given to WriteTrace in Memory component is empty.");
    ASSERT_RELEASE(
        address.size() == sorted_address_.Size(trace[0].size()),
        "Address size mismatches size of sorted address virtual column.");
    ASSERT_RELEASE(
        value.size() == sorted_value_.Size(trace[0].size()),
        "Value size mismatches size of sorted address virtual column.");
  }
  // Create sorted address value pairs.
  const auto sorted_address_value = AddressBasedSort<FieldElementT>(address, value);

  if (!disable_asserts) {
    ASSERT_RELEASE(
        sorted_address_value.size() == address.size(),
        "Wrong size of sorted_address_value: " + std::to_string(sorted_address_value.size()));
  }
  // Fill trace.
  for (size_t i = 0; i < sorted_address_value.size(); ++i) {
    if (i > 0 && !disable_asserts) {
      ValidateConsecutiveCells(sorted_address_value[i - 1], sorted_address_value[i], i - 1);
    }
    sorted_address_.SetCell(trace, i, FieldElementT::FromUint(sorted_address_value[i].first));
    sorted_value_.SetCell(trace, i, *(sorted_address_value[i].second));
  }

  return MemoryComponentProverContext1<FieldElementT>(
      std::move(address), std::move(value), std::move(public_memory_indices),
      std::move(multi_column_perm_component_));
}

template <typename FieldElementT>
void MemoryComponentProverContext1<FieldElementT>::WriteTrace(
    gsl::span<const FieldElementT> interaction_elms,
    gsl::span<const gsl::span<FieldElementT>> interaction_trace,
    const FieldElementT& expected_public_memory_prod) && {
  // Sort address-value pairs again to avoid memory consumption.
  auto sorted_address_value_pair = AddressBasedSort<FieldElementT>(address_, value_);

  std::vector<FieldElementT> address_elements;
  std::vector<FieldElementT> address_sorted_elements;
  std::vector<FieldElementT> value_sorted_elements;
  for (size_t i = 0; i < address_.size(); ++i) {
    address_elements.push_back(FieldElementT::FromUint(address_[i]));
    address_sorted_elements.push_back(FieldElementT::FromUint(sorted_address_value_pair[i].first));
    value_sorted_elements.push_back(*(sorted_address_value_pair[i].second));
  }
  address_.clear();

  std::vector<std::vector<FieldElementT>> unsorted_address_value;
  unsorted_address_value.reserve(2);
  unsorted_address_value.push_back(std::move(address_elements));
  unsorted_address_value.emplace_back(value_.begin(), value_.end());
  value_.clear();

  // The address-value pairs of the public memory were replaced with zeros in the first trace. Here
  // we apply this replacement on unsorted_address_value before passing it to
  // multi_column_perm_component_.WriteInteractionTrace() in order to be able to correctly compute
  // the public memory product.
  for (const auto& idx : public_memory_indices_) {
    unsorted_address_value[0][idx] = FieldElementT::Zero();
    unsorted_address_value[1][idx] = FieldElementT::Zero();
  }

  std::vector<std::vector<FieldElementT>> sorted_address_value;
  sorted_address_value.reserve(2);
  sorted_address_value.push_back(std::move(address_sorted_elements));
  sorted_address_value.push_back(std::move(value_sorted_elements));

  std::vector<gsl::span<const FieldElementT>> orig_spans(
      unsorted_address_value.begin(), unsorted_address_value.end());

  std::vector<gsl::span<const FieldElementT>> perm_spans(
      sorted_address_value.begin(), sorted_address_value.end());

  multi_column_perm_component_.WriteInteractionTrace(
      ConstSpanAdapter<gsl::span<const FieldElementT>>({orig_spans}),
      ConstSpanAdapter<gsl::span<const FieldElementT>>({perm_spans}), interaction_elms,
      interaction_trace, expected_public_memory_prod);
}

};  // namespace starkware
