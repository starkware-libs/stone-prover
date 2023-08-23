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


#include "starkware/math/math.h"

namespace starkware {

namespace composition_polynomial_multiplicative_iterator {
namespace details {

template <typename FieldElementT>
size_t GetCosetSize(gsl::span<const gsl::span<const FieldElementT>> trace_lde) {
  ASSERT_RELEASE(!trace_lde.empty(), "Trace must contain at least one column.");
  size_t coset_size = trace_lde[0].size();
  for (const auto& column : trace_lde) {
    ASSERT_RELEASE(column.size() == coset_size, "All columns must have the same size.");
  }
  return coset_size;
}

}  // namespace details
}  // namespace composition_polynomial_multiplicative_iterator

template <typename FieldElementT>
MultiplicativeNeighbors<FieldElementT>::MultiplicativeNeighbors(
    gsl::span<const std::pair<int64_t, uint64_t>> mask,
    gsl::span<const gsl::span<const FieldElementT>> trace_lde)
    : mask_(mask.begin(), mask.end()),
      coset_size_(composition_polynomial_multiplicative_iterator::details::GetCosetSize(trace_lde)),
      neighbor_wraparound_mask_(coset_size_ - 1),
      trace_lde_(trace_lde.begin(), trace_lde.end()),
      end_(this, coset_size_) {
  ASSERT_RELEASE(IsPowerOfTwo(coset_size_), "Coset size must be a power of 2");

  for (const auto& mask_item : mask) {
    ASSERT_RELEASE(mask_item.second < trace_lde_.size(), "Too few trace LDE columns provided.");
  }
}

template <typename FieldElementT>
MultiplicativeNeighbors<FieldElementT>::Iterator::Iterator(
    const MultiplicativeNeighbors* parent, size_t idx)
    : parent_(parent),
      idx_(idx),
      neighbors_(FieldElementT::UninitializedVector(parent->mask_.size())) {}

template <typename FieldElementT>
gsl::span<const FieldElementT> MultiplicativeNeighbors<FieldElementT>::Iterator::operator*() {
  const auto& mask = parent_->mask_;
  const auto& trace_lde = parent_->trace_lde_;
  const auto& neighbor_wraparound_mask = parent_->neighbor_wraparound_mask_;
  for (size_t i = 0; i < mask.size(); i++) {
    neighbors_[i] =
        trace_lde.at(mask[i].second).at((idx_ + mask[i].first) & neighbor_wraparound_mask);
  }
  return neighbors_;
}

}  // namespace starkware
