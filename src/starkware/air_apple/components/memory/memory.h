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

#ifndef STARKWARE_AIR_COMPONENTS_MEMORY_MEMORY_H_
#define STARKWARE_AIR_COMPONENTS_MEMORY_MEMORY_H_

#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/memory/memory_cell.h"
#include "starkware/air/components/permutation/multi_column_permutation.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"
#include "starkware/air/trace.h"

namespace starkware {

template <typename FieldElementT>
class MemoryComponentProverContext1;

template <typename FieldElementT>
class MemoryComponentProverContext {
 public:
  MemoryComponentProverContext() = delete;
  ~MemoryComponentProverContext() = default;
  MemoryComponentProverContext(const MemoryComponentProverContext& other) = delete;
  MemoryComponentProverContext& operator=(const MemoryComponentProverContext& other) = delete;
  MemoryComponentProverContext(MemoryComponentProverContext&& other) noexcept = default;
  MemoryComponentProverContext& operator=(MemoryComponentProverContext&& other) noexcept = delete;

  /*
    A component for a continuous read-only memory.
  */
  MemoryComponentProverContext(
      const std::string& name, const TraceGenerationContext& ctx,
      MemoryCell<FieldElementT>&& memory_cell)
      : sorted_address_(ctx.GetVirtualColumn(name + "/sorted/addr")),
        sorted_value_(ctx.GetVirtualColumn(name + "/sorted/value")),
        multi_column_perm_component_(name + "/multi_column_perm", 1, ctx),
        memory_cell_(std::move(memory_cell)) {}

  /*
    Writes the first trace (before the interaction) for the component.
    Destroys the object in the process.
    If disable_asserts is true it disables all the asserts of the function. This option should be
    used only for testing.
  */
  MemoryComponentProverContext1<FieldElementT> WriteTrace(
      gsl::span<const gsl::span<FieldElementT>> trace, bool disable_asserts = false) &&;

 private:
  /*
    A virtual column for the sorted address and value data.
  */
  const VirtualColumn sorted_address_;
  const VirtualColumn sorted_value_;

  /*
    The inner multi column permutation component.
  */
  MultiColumnPermutationComponent<FieldElementT> multi_column_perm_component_;

  /*
    Memory cell.
  */
  MemoryCell<FieldElementT> memory_cell_;
};

template <typename FieldElementT>
class MemoryComponentProverContext1 {
 public:
  MemoryComponentProverContext1() = delete;
  ~MemoryComponentProverContext1() = default;
  MemoryComponentProverContext1(const MemoryComponentProverContext1& other) = delete;
  MemoryComponentProverContext1& operator=(const MemoryComponentProverContext1& other) = delete;
  MemoryComponentProverContext1(MemoryComponentProverContext1&& other) noexcept = default;
  MemoryComponentProverContext1& operator=(MemoryComponentProverContext1&& other) noexcept = delete;

  MemoryComponentProverContext1(
      std::vector<uint64_t>&& address, std::vector<FieldElementT>&& value,
      std::vector<uint64_t>&& public_memory_indices,
      MultiColumnPermutationComponent<FieldElementT>&& multi_column_perm_component)
      : address_(std::move(address)),
        value_(std::move(value)),
        public_memory_indices_(std::move(public_memory_indices)),
        multi_column_perm_component_(multi_column_perm_component) {}

  /*
    Writes the interaction trace for the component.
    expected_public_memory_prod is the expected value of the public memory product which is the last
    element in the cum_prod column of the interaction trace.
  */
  void WriteTrace(
      gsl::span<const FieldElementT> interaction_elms,
      gsl::span<const gsl::span<FieldElementT>> interaction_trace,
      const FieldElementT& expected_public_memory_prod) &&;

 private:
  /*
    Values saved from previous interactions.
  */
  std::vector<uint64_t> address_;
  std::vector<FieldElementT> value_;

  /*
    Indices to the address_ and value_ vectors of the public memory data.
  */
  std::vector<uint64_t> public_memory_indices_;

  /*
    The inner multi column permutation component.
  */
  MultiColumnPermutationComponent<FieldElementT> multi_column_perm_component_;
};

}  // namespace starkware

#include "starkware/air/components/memory/memory.inl"

#endif  // STARKWARE_AIR_COMPONENTS_MEMORY_MEMORY_H_
