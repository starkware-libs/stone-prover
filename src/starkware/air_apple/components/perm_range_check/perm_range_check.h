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

#ifndef STARKWARE_AIR_COMPONENTS_PERM_RANGE_CHECK_PERM_RANGE_CHECK_H_
#define STARKWARE_AIR_COMPONENTS_PERM_RANGE_CHECK_PERM_RANGE_CHECK_H_

#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/perm_range_check/range_check_cell.h"
#include "starkware/air/components/permutation/permutation.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"
#include "starkware/air/trace.h"

namespace starkware {

template <typename FieldElementT>
class PermRangeCheckComponentProverContext1;

template <typename FieldElementT>
class PermRangeCheckComponentProverContext0 {
 public:
  // Delete constructors to avoid copying the object.
  ~PermRangeCheckComponentProverContext0() = default;
  PermRangeCheckComponentProverContext0(const PermRangeCheckComponentProverContext0& other) =
      delete;
  PermRangeCheckComponentProverContext0& operator=(
      const PermRangeCheckComponentProverContext0& other) = delete;
  PermRangeCheckComponentProverContext0(PermRangeCheckComponentProverContext0&& other) noexcept =
      default;
  PermRangeCheckComponentProverContext0& operator=(
      PermRangeCheckComponentProverContext0&& other) noexcept = delete;

  /*
    A component for a permutation based range check.
  */
  PermRangeCheckComponentProverContext0(
      const std::string& name, const TraceGenerationContext& ctx,
      RangeCheckCell<FieldElementT>&& range_check_cell)
      : sorted_column_(ctx.GetVirtualColumn(name + "/sorted")),
        perm_component_(name + "/perm", 1, ctx),
        range_check_cell_(std::move(range_check_cell)) {}

  /*
    Writes the trace for the component.
    Destroys the object in the process.
  */
  PermRangeCheckComponentProverContext1<FieldElementT> WriteTrace(
      gsl::span<const gsl::span<FieldElementT>> trace) &&;

 private:
  /*
    A virtual column for the sorted permutation of the data.
  */
  const VirtualColumn sorted_column_;

  /*
    The inner permutation component.
  */
  PermutationComponent<FieldElementT> perm_component_;

  /*
    Range check cell.
  */
  RangeCheckCell<FieldElementT> range_check_cell_;
};

template <typename FieldElementT>
class PermRangeCheckComponentProverContext1 {
 public:
  // Delete constructors to avoid copying the object.
  ~PermRangeCheckComponentProverContext1() = default;
  PermRangeCheckComponentProverContext1(const PermRangeCheckComponentProverContext1& other) =
      delete;
  PermRangeCheckComponentProverContext1& operator=(
      const PermRangeCheckComponentProverContext1& other) = delete;
  PermRangeCheckComponentProverContext1(PermRangeCheckComponentProverContext1&& other) noexcept =
      default;
  PermRangeCheckComponentProverContext1& operator=(
      PermRangeCheckComponentProverContext1&& other) noexcept = delete;

  PermRangeCheckComponentProverContext1(
      PermutationComponent<FieldElementT>&& perm_component, uint64_t actual_min,
      uint64_t actual_max, std::vector<uint64_t>&& data)
      : actual_min_(actual_min),
        actual_max_(actual_max),
        data_(data),
        perm_component_(perm_component) {}

  void WriteTrace(
      const FieldElementT& interaction_elm,
      gsl::span<const gsl::span<FieldElementT>> interaction_trace) const;

  uint64_t GetActualMin() const { return actual_min_; }
  uint64_t GetActualMax() const { return actual_max_; }

 private:
  const uint64_t actual_min_;
  const uint64_t actual_max_;

  /*
    Values saved from previous interactions.
  */
  std::vector<uint64_t> data_;
  PermutationComponent<FieldElementT> perm_component_;
};

}  // namespace starkware

#include "starkware/air/components/perm_range_check/perm_range_check.inl"

#endif  // STARKWARE_AIR_COMPONENTS_PERM_RANGE_CHECK_PERM_RANGE_CHECK_H_
