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

#ifndef STARKWARE_AIR_COMPONENTS_DILUTED_CHECK_DILUTED_CHECK_CELL_H_
#define STARKWARE_AIR_COMPONENTS_DILUTED_CHECK_DILUTED_CHECK_CELL_H_

#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "starkware/air/components/perm_table_check/table_check_cell.h"
#include "starkware/air/components/trace_generation_context.h"

namespace starkware {
namespace diluted_check_cell {

/*
  Converts a number to diluted form: adds spacing between its bits.
  Example: 0b1101 with spacing 3 turns into 0b 001 001 000 001.
  Assumes the input is in range [0, 2^n_bits).
*/
inline uint64_t Dilute(uint64_t x, size_t spacing, size_t n_bits) {
  uint64_t res = 0;
  for (size_t i = 0; i < n_bits; ++i) {
    res |= (x & Pow2(i)) << ((spacing - 1) * i);
  }
  return res;
}

/*
  Converts a number from diluted form. The inverse of Dilute().
  Assumes the input is in a diluted form of a number in the range [0, 2^n_bits).
*/
inline uint64_t Undilute(uint64_t x, size_t spacing, size_t n_bits) {
  uint64_t res = 0;
  for (size_t i = 0; i < n_bits; ++i) {
    res |= (x & Pow2(spacing * i)) >> ((spacing - 1) * i);
  }
  return res;
}

/*
  A table cell class for the diluted component that checks that numbers are of the form:
    \sum_{i=0}^{n_bits-1) b_i 2^(spacing * i).
*/
template <typename FieldElementT>
class DilutedCheckCell : public TableCheckCell<FieldElementT> {
 public:
  DilutedCheckCell(
      const std::string& name, const TraceGenerationContext& ctx, uint64_t trace_length,
      size_t spacing, size_t n_bits)
      : TableCheckCell<FieldElementT>(name, ctx, trace_length),
        spacing_(spacing),
        n_bits_(n_bits) {}

  /*
    Fills holes in unused cells.
  */
  void Finalize(gsl::span<const gsl::span<FieldElementT>> trace);

 private:
  size_t spacing_;
  size_t n_bits_;
};

}  // namespace diluted_check_cell
}  // namespace starkware

#include "starkware/air/components/diluted_check/diluted_check_cell.inl"

#endif  // STARKWARE_AIR_COMPONENTS_DILUTED_CHECK_DILUTED_CHECK_CELL_H_
