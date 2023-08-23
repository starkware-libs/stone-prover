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

#ifndef STARKWARE_COMPOSITION_POLYNOMIAL_MULTIPLICATIVE_NEIGHBORS_H_
#define STARKWARE_COMPOSITION_POLYNOMIAL_MULTIPLICATIVE_NEIGHBORS_H_

#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/error_handling/error_handling.h"

namespace starkware {

template <typename FieldElementT>
class MultiplicativeNeighbors {
 public:
  /*
    Constructs a MultiplicativeNeighbors instance.
    trace_lde should point to one coset in the LDE of the trace, and MUST be kept alive as long the
    iterator is alive.
    mask is a list of pairs (relative row, column), as defined by the AIR.
    trace_lde is a list of columns representing the LDE of the trace in some coset, in a natural
    order.
  */
  MultiplicativeNeighbors(
      gsl::span<const std::pair<int64_t, uint64_t>> mask,
      gsl::span<const gsl::span<const FieldElementT>> trace_lde);

  // Disable copy-constructor and operator= since end_ refers to this.
  MultiplicativeNeighbors(const MultiplicativeNeighbors& other) = delete;
  MultiplicativeNeighbors& operator=(const MultiplicativeNeighbors& other) = delete;
  MultiplicativeNeighbors(MultiplicativeNeighbors&& other) noexcept = delete;
  MultiplicativeNeighbors& operator=(MultiplicativeNeighbors&& other) noexcept = delete;

  ~MultiplicativeNeighbors() = default;

  class Iterator;

  /*
    Note: Destroying this instance will invalidate the iterator.
  */
  Iterator begin() const { return Iterator(this, 0); }  // NOLINT: invalid case style.
  const Iterator& end() const { return end_; }          // NOLINT: invalid case style.

  uint64_t CosetSize() const { return coset_size_; }

 private:
  const std::vector<std::pair<int64_t, uint64_t>> mask_;
  const uint64_t coset_size_;
  /*
    Precomputed value to allow computing (x % coset_size_) using an & operation.
  */
  const size_t neighbor_wraparound_mask_;
  const std::vector<gsl::span<const FieldElementT>> trace_lde_;
  /*
    Keep one instace of the iterator at the end, so that calls to end() will not allocate memory.
  */
  const Iterator end_;
};

template <typename FieldElementT>
class MultiplicativeNeighbors<FieldElementT>::Iterator {
 public:
  Iterator(const MultiplicativeNeighbors* parent, size_t idx);

  bool operator==(const Iterator& other) const {
    ASSERT_DEBUG(
        parent_ == other.parent_, "Comparing iterators with different parent_ is not allowed.");
    return idx_ == other.idx_;
  }
  bool operator!=(const Iterator& other) const { return !((*this) == other); }

  Iterator& operator+=(size_t offset) {
    idx_ += offset;
    return *this;
  }

  Iterator& operator++() {
    idx_++;
    return *this;
  }

  /*
    Returns the values of the neighbors.
    Calling operator*() twice is not recommended as it will copy the values twice.
    Returns a span for an internal storage of the iterator, which is invalidated once operator++ is
    called or the iterator is destroyed.
  */
  gsl::span<const FieldElementT> operator*();

 private:
  const MultiplicativeNeighbors* parent_;

  /*
    Index of the current point.
  */
  size_t idx_ = 0;

  /*
    Pre-allocated space for neighbor values.
  */
  std::vector<FieldElementT> neighbors_;
};

}  // namespace starkware

#include "multiplicative_neighbors.inl"

#endif  // STARKWARE_COMPOSITION_POLYNOMIAL_MULTIPLICATIVE_NEIGHBORS_H_
