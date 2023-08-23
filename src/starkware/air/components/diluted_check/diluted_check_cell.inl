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

#include <optional>
#include <string>

#include "glog/logging.h"

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {
namespace diluted_check_cell {

template <typename FieldElementT>
void DilutedCheckCell<FieldElementT>::Finalize(gsl::span<const gsl::span<FieldElementT>> trace) {
  // Find all used values.
  const uint64_t total_size = Pow2(n_bits_);
  // value_set is the set of values in range [0, 2^n_bits) whose diluted value appears in the cell.
  std::vector<bool> value_set(total_size);
  for (uint64_t i = 0; i < this->values_.size(); ++i) {
    if (!this->is_initialized_[i]) {
      continue;
    }
    const auto& value = this->values_[i];
    const uint64_t undiluted_value = Undilute(value, spacing_, n_bits_);
    ASSERT_RELEASE(
        Dilute(undiluted_value, spacing_, n_bits_) == value,
        "Invalid diluted value: " + std::to_string(value));
    value_set.at(undiluted_value) = true;
  }

  // Fill missing values.
  // current_value refers to the maximum value in the range [0, 2^n_bits) for which we know that all
  // previous values appear in the trace in diluted form.
  // At the beginnin, we know of no value, so we initialize it with 0.
  uint64_t current_value = 0;
  uint64_t filled_missings = 0;
  for (uint64_t i = 0; i < this->values_.size(); ++i) {
    if (this->is_initialized_[i]) {
      continue;
    }
    // Position i is free to fill. Find the next missing value in the value set to fill the position
    // with.
    // If we reached the end of the value set, keep current_value at the last value, so we can use
    // it to fill all the empty positions.
    for (; current_value != total_size - 1; ++current_value) {
      if (!value_set.at(current_value)) {
        break;
      }
    }

    // Put the missing value current_value at position i.
    value_set[current_value] = true;
    this->WriteTrace(i, Dilute(current_value, spacing_, n_bits_), trace);
    ++filled_missings;
  }

  // Find the number of still missing values.
  size_t still_missing = 0;
  for (; current_value != total_size; ++current_value) {
    if (!value_set.at(current_value)) {
      ++still_missing;
    }
  }

  if (still_missing > 0) {
    // We didn't have enough space to fill all the missings.
    THROW_STARKWARE_EXCEPTION(
        "Trace size is not large enough for dilluted-check values. Filled missing values: " +
        std::to_string(filled_missings) +
        ". Remaining missing values: " + std::to_string(still_missing) + ".");
  }
}

}  // namespace diluted_check_cell
}  // namespace starkware
