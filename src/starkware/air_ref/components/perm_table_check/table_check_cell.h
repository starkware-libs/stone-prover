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

#ifndef STARKWARE_AIR_COMPONENTS_PERM_TABLE_CHECK_TABLE_CHECK_CELL_H_
#define STARKWARE_AIR_COMPONENTS_PERM_TABLE_CHECK_TABLE_CHECK_CELL_H_

#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "starkware/air/components/trace_generation_context.h"

namespace starkware {

/*
  A cell class for components that verify that its values are in some specific set. Owns a virtual
  column used to hold the values in the component (but not the check). Should be used as a base
  class, and dervied classes should implement a Finalize() method to fill all the holes.
*/
template <typename FieldElementT>
class TableCheckCell {
 public:
  ~TableCheckCell() = default;
  TableCheckCell(const TableCheckCell& other) = delete;
  // Disable copy constructor to avoid unintentional copying.
  TableCheckCell& operator=(const TableCheckCell& other) = delete;
  TableCheckCell(TableCheckCell&& other) noexcept = default;  // NOLINT.
  TableCheckCell& operator=(TableCheckCell&& other) noexcept = delete;

  TableCheckCell(const std::string& name, const TraceGenerationContext& ctx, uint64_t trace_length)
      : vc_(ctx.GetVirtualColumn(name)),
        values_(vc_.Size(trace_length)),
        is_initialized_(vc_.Size(trace_length), false) {}

  /*
    Gets a relative view from a subview of this components' view. This is used primarilly by
    TableCheckCellView.
  */
  RowView GetRelativeSubview(const RowView& subview) { return vc_.view.Relative(subview); }
  uint64_t Size() const { return values_.size(); }

  /*
    Writes the value to the trace. Saves the values for the interaction phase.
  */
  void WriteTrace(uint64_t index, uint64_t value, gsl::span<const gsl::span<FieldElementT>> trace);

  std::vector<uint64_t> Consume() && { return std::move(values_); }

  uint64_t Get(uint64_t index) const {
    ASSERT_RELEASE(is_initialized_.at(index), "Uninitialized value");
    return values_[index];
  }

 private:
  /*
    A virtual column for the address and value data.
  */
  const VirtualColumn vc_;

  // Use std::unique_ptr<std::mutex> to allow moving this class.
  std::unique_ptr<std::mutex> write_trace_lock_ = std::make_unique<std::mutex>();

 protected:
  std::vector<uint64_t> values_;
  std::vector<bool> is_initialized_;
};

template <typename FieldElementT>
class TableCheckCellView {
 public:
  TableCheckCellView(TableCheckCell<FieldElementT>* parent, const RowView view)
      : parent_(parent), view_(view) {}

  // NOLINTNEXTLINE: clang-tidy false positive. Uninitialized member view_.
  TableCheckCellView(
      TableCheckCell<FieldElementT>* const parent, const std::string& name,
      const TraceGenerationContext& ctx)
      : TableCheckCellView(parent, parent->GetRelativeSubview(ctx.GetVirtualColumn(name).view)) {}

  void WriteTrace(
      uint64_t index, const uint64_t value, gsl::span<const gsl::span<FieldElementT>> trace) const {
    parent_->WriteTrace(view_.At(index), value, trace);
  }

  uint64_t Get(uint64_t index) const { return parent_->Get(view_.At(index)); }

 private:
  /*
    The parent memory cell.
  */
  TableCheckCell<FieldElementT>* parent_;

  /*
    A virtual view mapping this view into the memory cell.
  */
  const RowView view_;
};

}  // namespace starkware

#include "starkware/air/components/perm_table_check/table_check_cell.inl"

#endif  // STARKWARE_AIR_COMPONENTS_PERM_TABLE_CHECK_TABLE_CHECK_CELL_H_
