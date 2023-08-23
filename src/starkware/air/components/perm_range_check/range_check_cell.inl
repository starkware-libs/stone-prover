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

template <typename FieldElementT>
void RangeCheckCell<FieldElementT>::Finalize(
    const uint64_t rc_min, const uint64_t rc_max, gsl::span<const gsl::span<FieldElementT>> trace) {
  ASSERT_RELEASE(rc_min < rc_max, "rc_min must be smaller than rc_max");
  ASSERT_RELEASE(
      rc_max < std::numeric_limits<uint64_t>::max(),
      "rc_max must be smaller than " + std::to_string(std::numeric_limits<uint64_t>::max()));

  // Find all used values.
  std::vector<bool> value_set(rc_max - rc_min + 1);
  for (uint64_t i = 0; i < this->values_.size(); ++i) {
    const auto& value = this->values_[i];
    if (!this->is_initialized_[i]) {
      continue;
    }
    ASSERT_RELEASE(
        value >= rc_min && value <= rc_max, "Out of range value: " + std::to_string(value) +
                                                ", min=" + std::to_string(rc_min) +
                                                ", max=" + std::to_string(rc_max));
    value_set.at(value - rc_min) = true;
  }

  // Fill holes.
  // last_hole refers to the last value in the range [rc_min, rc_max] which did not appear naturally
  // in the trace. Initialize it to rc_min - 1.
  uint64_t last_hole = rc_min - 1;
  uint64_t filled_holes = 0;
  for (uint64_t i = 0; i < this->values_.size(); ++i) {
    if (this->is_initialized_[i]) {
      continue;
    }
    // Find the next hole.
    while (last_hole != rc_max) {
      last_hole++;
      if (!value_set.at(last_hole - rc_min)) {
        break;
      }
    }

    this->WriteTrace(i, last_hole, trace);
    filled_holes++;
  }

  size_t remaining_holes = 0;
  last_hole++;
  for (; last_hole < rc_max + 1; last_hole++) {
    if (!value_set.at(last_hole - rc_min)) {
      remaining_holes++;
    }
  }

  if (remaining_holes > 0) {
    // We didn't have enough space to fill all the holes.
    THROW_STARKWARE_EXCEPTION(
        "Trace size is not large enough for range-check values. Range size: " +
        std::to_string(rc_max - rc_min + 1) + ". Filled Holes: " + std::to_string(filled_holes) +
        ". Remaining holes: " + std::to_string(remaining_holes) + ".");
  }
}

}  // namespace starkware
