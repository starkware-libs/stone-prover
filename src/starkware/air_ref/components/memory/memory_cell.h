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

#ifndef STARKWARE_AIR_COMPONENTS_MEMORY_MEMORY_CELL_H_
#define STARKWARE_AIR_COMPONENTS_MEMORY_MEMORY_CELL_H_

#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "starkware/air/components/trace_generation_context.h"

namespace starkware {

/*
  A memory cell component. Owns 2 virtual columns, address and value. Allows using subviews of the
  memory cell. This class also saves the necessary data for the memory component's interaction.
*/
template <typename FieldElementT>
class MemoryCell {
 public:
  ~MemoryCell() = default;
  MemoryCell(const MemoryCell& other) = delete;
  // Disable copy constructor to avoid unintentional copying.
  MemoryCell& operator=(const MemoryCell& other) = delete;
  MemoryCell(MemoryCell&& other) noexcept = default;  // NOLINT.
  MemoryCell& operator=(MemoryCell&& other) noexcept = delete;

  MemoryCell(const std::string& name, const TraceGenerationContext& ctx, uint64_t trace_length)
      : address_vc_(ctx.GetVirtualColumn(name + "/addr")),
        value_vc_(ctx.GetVirtualColumn(name + "/value")),
        address_(address_vc_.Size(trace_length)),
        value_(value_vc_.Size(trace_length), FieldElementT::Zero()),
        is_initialized_(address_vc_.Size(trace_length), false),
        address_min_(std::numeric_limits<uint64_t>::max()),
        address_max_(0) {}

  /*
    Gets a relative view from a subview of this components' view. This is used primarilly by
    MemoryCellView.
  */
  RowView GetRelativeSubview(const RowView& subview) { return address_vc_.view.Relative(subview); }
  uint64_t Size() const { return address_.size(); }

  /*
    Writes the address and value to the trace. Saves the address and value for the interaction
    phase. is_public_memory is an indicator that the given address-value pair is part of the public
    memory.
  */
  void WriteTrace(
      uint64_t index, uint64_t address, const FieldElementT& value,
      gsl::span<const gsl::span<FieldElementT>> trace, bool is_public_memory);

  std::tuple<std::vector<uint64_t>, std::vector<FieldElementT>, std::vector<uint64_t>>
  Consume() && {
    return {std::move(address_), std::move(value_), std::move(public_input_indices_)};
  }

  /*
    Writes dummy values for all the unused memory units, filling address gaps if necessary.
    If disable_asserts is true it disables all the asserts of the function. This option should be
    used only for testing.
  */
  void Finalize(gsl::span<const gsl::span<FieldElementT>> trace, bool disable_asserts = false);

 private:
  /*
    A virtual column for the address and value data.
  */
  const VirtualColumn address_vc_;
  const VirtualColumn value_vc_;

  std::vector<uint64_t> address_;
  std::vector<FieldElementT> value_;
  std::vector<bool> is_initialized_;
  std::vector<uint64_t> public_input_indices_;

  uint64_t address_min_;
  uint64_t address_max_;

  // Use std::unique_ptr<std::mutex> to allow moving this class.
  std::unique_ptr<std::mutex> write_trace_lock_ = std::make_unique<std::mutex>();
};

template <typename FieldElementT>
class MemoryCellView {
 public:
  MemoryCellView(MemoryCell<FieldElementT>* parent, const RowView view)
      : parent_(parent), view_(view) {}

  // NOLINTNEXTLINE: clang-tidy false positive. Uninitialized member view_.
  MemoryCellView(
      MemoryCell<FieldElementT>* const parent, const std::string& name,
      const TraceGenerationContext& ctx)
      : MemoryCellView(
            parent, parent->GetRelativeSubview(ctx.GetVirtualColumn(name + "/addr").view)) {}

  void WriteTrace(
      uint64_t index, const uint64_t address, const FieldElementT& value,
      gsl::span<const gsl::span<FieldElementT>> trace, bool is_public_memory = false) const {
    parent_->WriteTrace(view_.At(index), address, value, trace, is_public_memory);
  }

  uint64_t Size() const { return view_.Size(parent_->Size()); }

 private:
  /*
    The parent memory cell.
  */
  MemoryCell<FieldElementT>* parent_;

  /*
    A virtual view mapping this view into the memory cell.
  */
  const RowView view_;
};

}  // namespace starkware

#include "starkware/air/components/memory/memory_cell.inl"

#endif  // STARKWARE_AIR_COMPONENTS_MEMORY_MEMORY_CELL_H_
