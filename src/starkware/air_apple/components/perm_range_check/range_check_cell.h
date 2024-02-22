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

#ifndef STARKWARE_AIR_COMPONENTS_PERM_RANGE_CHECK_RANGE_CHECK_CELL_H_
#define STARKWARE_AIR_COMPONENTS_PERM_RANGE_CHECK_RANGE_CHECK_CELL_H_

#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "starkware/air/components/perm_table_check/table_check_cell.h"
#include "starkware/air/components/trace_generation_context.h"

namespace starkware {

template <typename FieldElementT>
class RangeCheckCell : public TableCheckCell<FieldElementT> {
 public:
  ~RangeCheckCell() = default;
  RangeCheckCell(const RangeCheckCell& other) = delete;
  // Disable copy constructor to avoid unintentional copying.
  RangeCheckCell& operator=(const RangeCheckCell& other) = delete;
  RangeCheckCell(RangeCheckCell&& other) noexcept = default;  // NOLINT.
  RangeCheckCell& operator=(RangeCheckCell&& other) noexcept = delete;

  RangeCheckCell(const std::string& name, const TraceGenerationContext& ctx, uint64_t trace_length)
      : TableCheckCell<FieldElementT>(name, ctx, trace_length) {}

  /*
    Fills holes in unused cells.
    These unused cells will be assigned values to fill holes in the range [rc_min, rc_max].
    For example, if rc_min=2, but the value 3 does not appear naturally, then one of the unused
    cells will contain 3.
  */
  void Finalize(uint64_t rc_min, uint64_t rc_max, gsl::span<const gsl::span<FieldElementT>> trace);
};

}  // namespace starkware

#include "starkware/air/components/perm_range_check/range_check_cell.inl"

#endif  // STARKWARE_AIR_COMPONENTS_PERM_RANGE_CHECK_RANGE_CHECK_CELL_H_
