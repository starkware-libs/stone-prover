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
void MemoryCell<FieldElementT>::WriteTrace(
    const uint64_t index, const uint64_t address, const FieldElementT& value,
    const gsl::span<const gsl::span<FieldElementT>> trace, bool is_public_memory) {
  std::unique_lock lock(*write_trace_lock_);
  ASSERT_RELEASE(
      is_initialized_[index] == false,
      "Memory unit " + std::to_string(index) + " was already written.");
  is_initialized_[index] = true;
  address_[index] = address;
  value_[index] = value;

  // Update address range.
  address_min_ = std::min(address, address_min_);
  address_max_ = std::max(address, address_max_);

  // In case is_public_memory is true, write (0,0) instead of (address, value) in the vc's.
  address_vc_.SetCell(
      trace, index, is_public_memory ? FieldElementT::Zero() : FieldElementT::FromUint(address));
  value_vc_.SetCell(trace, index, is_public_memory ? FieldElementT::Zero() : value);
  if (is_public_memory) {
    public_input_indices_.push_back(index);
  }
}

template <typename FieldElementT>
void MemoryCell<FieldElementT>::Finalize(
    gsl::span<const gsl::span<FieldElementT>> trace, bool disable_asserts) {
  if (!disable_asserts) {
    ASSERT_RELEASE(address_min_ <= address_max_, "address_min_ must be smaller than address_max_");
  }

  // Find all used addresses.
  std::vector<bool> address_set(address_max_ - address_min_ + 1);
  for (size_t index = 0; index < is_initialized_.size(); ++index) {
    if (is_initialized_[index]) {
      const uint64_t address = address_[index];
      if (!disable_asserts) {
        ASSERT_RELEASE(
            address >= address_min_ && address <= address_max_,
            "Out of range address: " + std::to_string(address) +
                ", min=" + std::to_string(address_min_) + ", max=" + std::to_string(address_max_));
      }
      address_set.at(address - address_min_) = true;
    }
  }

  // Fill holes.
  // last_hole refers to an address in [address_min_, address_max + 1] such that all addresses
  // in [address_min_, last_hole) appear in memory. It is initialized to address_min_, and whenever
  // an empty memory cell is encountered, it is advanced to the next largest such address (either a
  // hole or address_max_+1). If a hole is filled, it is then increased by 1.
  // We copy adress_max_ before filling holes, as its value will increase if
  // WriteTrace(index, address_max_ + 1, 0, ... ) is called.
  uint64_t last_hole = address_min_;
  const uint64_t orig_address_max = address_max_;
  uint64_t filled_holes = 0;
  uint64_t vacancies_filled = 0;
  for (size_t index = 0; index < is_initialized_.size(); ++index) {
    if (is_initialized_[index]) {
      continue;
    }

    // Find next hole.
    for (; last_hole <= orig_address_max; ++last_hole) {
      if (!address_set.at(last_hole - address_min_)) {
        break;
      }
    }

    // Fill hole.
    WriteTrace(index, last_hole, FieldElementT::Zero(), trace, false);
    if (last_hole <= orig_address_max) {
      ++last_hole;
      ++filled_holes;
    }

    ++vacancies_filled;
  }

  // Check whether any holes remain.
  size_t remaining_holes = 0;
  for (; last_hole <= orig_address_max; ++last_hole) {
    if (!address_set.at(last_hole - address_min_)) {
      ++remaining_holes;
    }
  }

  if (remaining_holes > 0 && !disable_asserts) {
    // We didn't have enough space to fill all the holes.
    THROW_STARKWARE_EXCEPTION(
        "Available memory size was not large enough to fill holes in memory address range. Memory "
        "address range: " +
        std::to_string(orig_address_max - address_min_ + 1) +
        ". Filled holes: " + std::to_string(filled_holes) +
        ". Remaining holes: " + std::to_string(remaining_holes) + ".");
  }

  VLOG(0) << "Filled " + std::to_string(vacancies_filled) +
                 " vacant slots in memory: " + std::to_string(filled_holes) + " holes and " +
                 std::to_string(vacancies_filled - filled_holes) + " spares."
          << std::endl;
}

}  // namespace starkware
