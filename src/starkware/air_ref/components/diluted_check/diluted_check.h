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

#ifndef STARKWARE_AIR_COMPONENTS_DILUTED_CHECK_DILUTED_CHECK_H_
#define STARKWARE_AIR_COMPONENTS_DILUTED_CHECK_DILUTED_CHECK_H_

#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/diluted_check/diluted_check_cell.h"
#include "starkware/air/components/permutation/permutation.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/field_operations.h"

namespace starkware {

template <typename FieldElementT>
class DilutedCheckComponentProverContext1;

template <typename FieldElementT>
class DilutedCheckComponentProverContext0 {
  using DilutedCheckCellT = diluted_check_cell::DilutedCheckCell<FieldElementT>;

 public:
  // Delete constructors to avoid copying the object.
  ~DilutedCheckComponentProverContext0() = default;
  DilutedCheckComponentProverContext0(const DilutedCheckComponentProverContext0& other) = delete;
  DilutedCheckComponentProverContext0& operator=(
      const DilutedCheckComponentProverContext0& other) noexcept = delete;
  // NOLINTNEXTLINE: noexcept.
  DilutedCheckComponentProverContext0(DilutedCheckComponentProverContext0&& other) = default;
  DilutedCheckComponentProverContext0& operator=(
      DilutedCheckComponentProverContext0&& other) noexcept = delete;

  /*
    A component for a permutation based diluted check.
  */
  DilutedCheckComponentProverContext0(
      const std::string& name, size_t spacing, size_t n_bits, const TraceGenerationContext& ctx,
      DilutedCheckCellT&& diluted_check_cell)
      : spacing_(spacing),
        n_bits_(n_bits),
        sorted_column_(ctx.GetVirtualColumn(name + "/permuted_values")),
        cum_val_col_(ctx.GetVirtualColumn(name + "/cumulative_value")),
        perm_component_(name + "/permutation", 1, ctx),
        diluted_check_cell_(std::move(diluted_check_cell)) {}

  /*
    Writes the trace for the component.
    Destroys the object in the process.
  */
  DilutedCheckComponentProverContext1<FieldElementT> WriteTrace(
      gsl::span<const gsl::span<FieldElementT>> trace) &&;

 private:
  /*
    The space between representation bits.
  */
  const size_t spacing_;

  /*
    The number of representation bits.
  */
  const size_t n_bits_;

  /*
    A virtual column for the sorted permutation of the data.
  */
  const VirtualColumn sorted_column_;

  /*
    A virtual column for the cumulative value.
  */
  const VirtualColumn cum_val_col_;

  /*
    The inner permutation component.
  */
  const PermutationComponent<FieldElementT> perm_component_;

  /*
    Diluted check cell.
  */
  DilutedCheckCellT diluted_check_cell_;
};

template <typename FieldElementT>
class DilutedCheckComponentProverContext1 {
 public:
  // Delete constructors to avoid copying the object.
  ~DilutedCheckComponentProverContext1() = default;
  DilutedCheckComponentProverContext1(const DilutedCheckComponentProverContext1& other) = delete;
  DilutedCheckComponentProverContext1& operator=(
      const DilutedCheckComponentProverContext1& other) noexcept = delete;
  // NOLINTNEXTLINE: noexcept.
  DilutedCheckComponentProverContext1(DilutedCheckComponentProverContext1&& other) = default;
  DilutedCheckComponentProverContext1& operator=(
      DilutedCheckComponentProverContext1&& other) noexcept = delete;

  DilutedCheckComponentProverContext1(
      size_t spacing, size_t n_bits, const PermutationComponent<FieldElementT>&& perm_component,
      std::vector<uint64_t>&& data, const VirtualColumn&& cum_val_col)
      : spacing_(spacing),
        n_bits_(n_bits),
        data_(data),
        cum_val_col_(cum_val_col),
        perm_component_(perm_component) {}

  void WriteTrace(
      FieldElementT perm_interaction_elm, FieldElementT interaction_z,
      FieldElementT interaction_alpha,
      gsl::span<const gsl::span<FieldElementT>> interaction_trace) const;

  /*
    Compute the final value of the cumulative value column.
  */
  static FieldElementT ExpectedFinalCumulativeValue(
      size_t spacing, size_t n_bits, const FieldElementT& interaction_z,
      const FieldElementT& interaction_alpha);

 private:
  /*
    The space between representation bits.
  */
  const size_t spacing_;

  /*
    The number of representation bits.
  */
  const size_t n_bits_;
  /*
    Values saved from previous interactions.
  */
  std::vector<uint64_t> data_;

  /*
    A virtual column for the cumulative value.
  */
  const VirtualColumn cum_val_col_;

  /*
    The inner permutation component.
  */
  const PermutationComponent<FieldElementT> perm_component_;
};

}  // namespace starkware

#include "starkware/air/components/diluted_check/diluted_check.inl"

#endif  // STARKWARE_AIR_COMPONENTS_DILUTED_CHECK_DILUTED_CHECK_H_
